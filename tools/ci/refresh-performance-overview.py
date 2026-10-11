"""Refresh the README overview and PNG inputs from published measurements."""
import argparse
import csv
from datetime import datetime
import importlib.util
import json
import math
from pathlib import Path
import re
import shutil
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from publication_metadata import sanitize_publication

BACKENDS = ("standard", "mimalloc", "jemalloc")
SCENARIOS = (
    ("create_destroy", 64, 1, 1),
    ("std_vector", 256, 8, 1),
    ("reallocate", 4096, 16, 1),
    ("cross_thread", 64, 16, 8),
)
WORKLOADS = (
    ("make_unique, 64 B", "object", 64, 1, 1),
    ("make_shared, 64 B", "shared_object", 64, 1, 1),
    ("vector, 32 integers", "std_vector", 256, 8, 1),
    ("PMR vector, 32 integers", "pmr_vector", 256, 8, 1),
    ("Zeroed Block, 4 KiB", "zeroed", 4096, 16, 1),
    ("Block resize, 4 to 8 KiB", "reallocate", 4096, 16, 1),
    ("Mixed lifetimes, 4-16 KiB", "mixed_lifetime", 4096, 16, 1),
    ("Cross-thread free, 8 threads", "cross_thread", 64, 16, 8),
)
PHASES = ("dense", "sparse", "churn_dense", "churn_sparse", "freed")


def read_csv(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def selected_median(rows, scenario):
    work, size, alignment, threads = scenario
    selected = [row for row in rows if row["workload"] == work and
                int(row["bytes"]) == size and int(row["alignment"]) == alignment and
                int(row["threads"]) == threads and row["statistics"] == "disabled" and
                row["heap"] == "no"]
    if len(selected) != 3 or {row["trial"] for row in selected} != {"1", "2", "3"}:
        raise ValueError(f"Incomplete overview scenario: {scenario}")
    values = [float(row["median_ns_per_operation"]) for row in selected]
    if any(not math.isfinite(value) or value <= 0 for value in values):
        raise ValueError("Invalid overview operation time")
    return statistics.median(values)


def load_tool(root, name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), root / "tools/ci" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def stage_artifacts(root, artifacts, revision):
    report = load_tool(root, "performance-report")
    staged = []
    for platform in ("linux-x64", "windows-x64", "macos-arm64"):
        for backend in BACKENDS:
            for mode in ("OFF", "ON"):
                source = artifacts / f"performance-{platform}-{backend}-{mode}" / "results"
                manifest = report.validate(source)
                if manifest["source_revision"] != revision:
                    raise ValueError("Overview artifact source does not match trusted revision")
                staged.append((source, root / "docs/results/current" / platform / backend / mode))
    # Validate every profile before replacing any published measurements.
    for source, destination in staged:
        destination.mkdir(parents=True, exist_ok=True)
        for name in ("environment.json", "sweep-environment.json", "full.csv", "latency.csv",
                     "scaling.csv", "tails.csv", "pressure.csv", "footprint.csv", "heap.csv"):
            if name.endswith(".json"):
                value = sanitize_publication(json.loads((source / name).read_text(encoding="utf-8")))
                (destination / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")
            else:
                shutil.copyfile(source / name, destination / name)


def comparison_data(root):
    result = {"source_revision": None, "platforms": {}}
    for platform, short in (("windows-x64", "windows"), ("linux-x64", "linux")):
        entries = [{"label": label, "workload": work, "values": {}}
                   for label, work, *_ in WORKLOADS]
        pressure, profiles, overview = [], {}, {}
        for backend in BACKENDS:
            folder = root / "docs/results/current" / platform / backend / "OFF"
            environment = json.loads((folder / "environment.json").read_text(encoding="utf-8"))
            sweep = json.loads((folder / "sweep-environment.json").read_text(encoding="utf-8"))
            revision = environment["source_revision"]
            if result["source_revision"] is None:
                result["source_revision"] = revision
            if revision != result["source_revision"]:
                raise ValueError("Overview mixes measured source revisions")
            profiles[backend] = {"environment": environment, "sweep_environment": sweep}
            rows = read_csv(folder / "full.csv")
            overview[backend] = [selected_median(rows, scenario) for scenario in SCENARIOS]
            for entry, (_, work, size, alignment, threads) in zip(entries, WORKLOADS):
                entry["values"][backend] = selected_median(rows, (work, size, alignment, threads))
            rows = read_csv(folder / "pressure.csv")
            for path in ("native", "disabled"):
                for phase in PHASES:
                    selected = [row for row in rows if row["path"] == path and row["phase"] == phase]
                    if len(selected) != 3 or {row["trial"] for row in selected} != {"1", "2", "3"}:
                        raise ValueError("Incomplete overview memory phase")
                    pressure.append({"backend": backend, "path": path, "phase": phase,
                        "requested_mib": statistics.median(int(row["requested_bytes"]) / 1048576 for row in selected),
                        "resident_delta_mib": statistics.median(
                            (int(row["rss"]) - int(row["baseline_rss"])) / 1048576 for row in selected)})
        result["platforms"][short] = {"workloads": entries, "pressure": pressure,
                                      "overview": overview, "profiles": profiles}
    return result


def refresh_overview(root, output, expected_revision=None):
    data = comparison_data(root)
    if expected_revision and data["source_revision"] != expected_revision:
        raise ValueError("Overview data source differs from requested revision")
    windows = data["platforms"]["windows"]
    dates = [datetime.fromisoformat(profile["sweep_environment"]["measured_at_utc"]).date()
             for profile in windows["profiles"].values()]
    measured = str(max(dates))
    versions = {profile["environment"]["compiler"]["CMAKE_CXX_COMPILER_VERSION"]
                for profile in windows["profiles"].values()}
    compiler = "MSVC " + "/".join(sorted({".".join(version.split(".")[:2]) for version in versions}))
    prepared = []
    for name, heading, header in (("README.md", "## Performance", "| Operation |"),
                                  ("README.zh-CN.md", "## \u6027\u80fd", "| \u64cd\u4f5c |")):
        path = root / name
        content = path.read_text(encoding="utf-8")
        begin = content.index(heading + "\n")
        finish = content.index("\n## ", begin + len(heading))
        block = content[begin:finish]
        lines = block.splitlines()
        first = next(index for index, line in enumerate(lines) if line.startswith(header))
        if len(lines[first:first + 6]) != 6 or any(not line.startswith("|") for line in lines[first:first + 6]):
            raise ValueError("README performance table no longer matches four-row overview")
        for index in range(4):
            label = lines[first + 2 + index].split("|")[1].strip()
            values = " | ".join(f"{windows['overview'][backend][index]:.1f}" for backend in BACKENDS)
            lines[first + 2 + index] = f"| {label} | {values} |"
        for index, line in enumerate(lines):
            if line.startswith("Windows x64 ·"):
                line = re.sub(r"MSVC [\d./]+", compiler, line)
                lines[index] = re.sub(r"\d{4}-\d{2}-\d{2}", measured, line)
        replacement = "\n".join(lines) + ("\n" if block.endswith("\n") else "")
        prepared.append((path, content[:begin] + replacement + content[finish:]))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8", newline="\n")
    for path, content in prepared:
        path.write_text(content, encoding="utf-8", newline="\n")
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--source-revision")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.artifacts:
        if not args.source_revision:
            parser.error("--artifacts requires --source-revision")
        stage_artifacts(root, args.artifacts.resolve(), args.source_revision)
    data = refresh_overview(root, args.output.resolve(), args.source_revision)
    if args.native_input:
        load_tool(root, "refresh-performance-tables").refresh_tables(root, args.native_input.resolve())
    print("Updated overview from measured source", data["source_revision"])


if __name__ == "__main__":
    main()
