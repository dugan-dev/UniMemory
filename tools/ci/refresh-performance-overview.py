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
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from publication_metadata import sanitize_publication
from result_io import read_csv, read_json, resolve_path

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
LOCAL_SOURCE = "docs/results/windows-local-comparison.json"
LOCAL_MARKER = f"<!-- performance-overview-source: {LOCAL_SOURCE} -->"
LOCAL_FACTORS = ("unique", "vector", "owned")


def validate_local_overview(root, block):
    """Validate the local table independently of the historical CI plot inputs."""
    if LOCAL_MARKER not in block:
        raise ValueError("Missing explicit local overview source marker")
    snapshot = read_json(root / LOCAL_SOURCE)
    if (snapshot.get("source") != "local-windows-comparison" or snapshot.get("sourceRoot") != "."
            or snapshot.get("trials") != 10 or snapshot.get("statistic") != "median"
            or snapshot.get("current_profile_verified") is not True):
        raise ValueError("Invalid local overview provenance")
    for backend in BACKENDS:
        profile = snapshot["profiles"][backend]
        actual = profile["compiled_profile"]
        if (profile["statistics"] != "OFF" or profile["configuration"] != "Release"
                or actual["actual_statistics"] != "OFF" or actual["actual_backend"] != backend
                or actual["actual_checks_enabled"] != "0" or actual["release_ipo"] not in ("ON", "TRUE", "1")):
            raise ValueError("Local overview requires verified Release/IPO, statistics OFF")
    factors = {"raw:" + str(size) for size in (16, 64, 256, 4096, 65536)}
    factors.update(("zeroed", "reallocate", "reallocate_zeroed", "unique", "array", "shared", "vector", "pmr_vector", "owned"))
    expected = {(backend, build, factor) for backend in BACKENDS for build in ("main", "current") for factor in factors}
    rows = snapshot["summary"]
    keys = [(row["backend"], row["build"], row["factor"]) for row in rows]
    if len(rows) != 84 or len(set(keys)) != 84 or set(keys) != expected:
        raise ValueError("Local overview requires every one of the 84 matched summaries")
    for row in rows:
        if int(row["trials"]) != 10 or any(not math.isfinite(float(row[field])) or float(row[field]) <= 0
                                         for field in ("control_median", "changed_median")):
            raise ValueError("Invalid local overview summary")
    table = [line for line in block.splitlines() if line.startswith("|")]
    if len(table) != 5:
        raise ValueError("Local overview requires its three-row table")
    for line, factor in zip(table[2:], LOCAL_FACTORS):
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        values = [next(row for row in rows if row["backend"] == backend and row["build"] == "current"
                       and row["factor"] == factor)["changed_median"] for backend in BACKENDS]
        if (len(cells) != 4 or cells[1:] != [f"{float(value):.1f}" for value in values]
                or (factor == "vector" and "256" not in cells[0])):
            raise ValueError(f"README local overview differs from its snapshot: {factor}")
    return snapshot


def selected_median(rows, scenario, trials=3, mean=False):
    work, size, alignment, threads = scenario
    selected = [row for row in rows if row["workload"] == work and
                int(row["bytes"]) == size and int(row["alignment"]) == alignment and
                int(row["threads"]) == threads and row["statistics"] == "disabled" and
                row["heap"] == "no"]
    if len(selected) != trials or {row["trial"] for row in selected} != {str(trial) for trial in range(1, trials + 1)}:
        raise ValueError(f"Incomplete overview scenario: {scenario}")
    values = [float(row["mean_ns_per_operation" if mean else "median_ns_per_operation"]) for row in selected]
    if any(not math.isfinite(value) or value <= 0 for value in values):
        raise ValueError("Invalid overview operation time")
    return statistics.mean(values) if mean else statistics.median(values)


def load_tool(root, name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), root / "tools/ci" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def stage_artifacts(root, artifacts, revision, expected_run_id=None):
    report = load_tool(root, "performance-report")
    staged = []
    groups = {}
    for platform in ("linux-x64", "windows-x64", "macos-arm64"):
        for backend in BACKENDS:
            for mode in ("OFF", "ON"):
                legacy = artifacts / f"performance-{platform}-{backend}-{mode}" / "results"
                candidates = [legacy] if (legacy / "environment.json").is_file() else []
                if not candidates:
                    for environment in (artifacts / f"performance-{platform}").rglob("environment.json"):
                        manifest = read_json(environment)
                        profile = manifest.get("build_profile", {})
                        if profile.get("backend") == backend and profile.get("statistics") == mode:
                            candidates.append(environment.parent)
                if len(candidates) != 1:
                    raise ValueError(f"Missing or duplicate overview artifact: {platform}/{backend}/{mode}")
                source = candidates[0]
                manifest = report.validate(source, strict_artifact=True)
                ledger = manifest.pop("_validated_tail_ledger", None)
                if manifest["source_revision"] != revision:
                    raise ValueError("Overview artifact source does not match trusted revision")
                if expected_run_id is not None and str(manifest.get("run_id")) != str(expected_run_id):
                    raise ValueError("Overview artifact run does not match trusted run ID")
                groups.setdefault(platform, []).append(manifest)
                staged.append((source, root / "docs/results/current" / platform / backend / mode, ledger))
    # Validate every profile before replacing any published measurements.
    for group in groups.values():
        report.validate_profile_group(group)
    for source, destination, ledger in staged:
        destination.mkdir(parents=True, exist_ok=True)
        files = ["environment.json", "sweep-environment.json", "full.csv", "latency.csv",
                 "scaling.csv", "tails.csv", "pressure.csv", "footprint.csv", "heap.csv"]
        if read_json(source / "environment.json")["schema"] == 3:
            files.extend(("summary.json", "controlled-suite.json", "sweep-operations.txt"))
        if read_json(source / "environment.json").get("measurement_protocol") == report.SUSTAINED_PROTOCOL:
            files.extend(("calibration-pilots.json", "full-calibration.json"))
        for name in files:
            actual = resolve_path(source / name)
            target = destination / (name if name.endswith(".json") else actual.name)
            alternate = destination / (name + ".gz" if target.name == name else name)
            if alternate.exists():
                alternate.unlink()
            if name.endswith(".json"):
                value = sanitize_publication(read_json(source / name))
                target.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")
            else:
                shutil.copyfile(actual, target)
        if ledger is not None:
            compressed = destination / "tail-traces.json.gz"
            if compressed.exists():
                compressed.unlink()
            (destination / "tail-traces.json").write_text(json.dumps(ledger, separators=(",", ":")) + "\n", encoding="utf-8", newline="\n")


def comparison_data(root):
    result = {"source_revision": None, "platforms": {}}
    report = load_tool(Path(__file__).resolve().parents[2], "performance-report")
    contract = None
    validated_platforms = set()
    for platform, short in (("windows-x64", "windows"), ("linux-x64", "linux")):
        entries = [{"label": label, "workload": work, "values": {}}
                   for label, work, *_ in WORKLOADS]
        pressure, profiles, overview = [], {}, {}
        for backend in BACKENDS:
            folder = root / "docs/results/current" / platform / backend / "OFF"
            environment = read_json(folder / "environment.json")
            count = report.trial_count(environment)
            mean = environment["schema"] == 3
            if mean and platform not in validated_platforms:
                report.load_combined(root / "docs/results/current" / platform)
                validated_platforms.add(platform)
            current_contract = (environment["schema"], count, environment.get("measurement_protocol"))
            if contract is not None and current_contract != contract:
                raise ValueError("Overview mixes measurement methods or trial counts")
            contract = current_contract
            sweep = read_json(folder / "sweep-environment.json")
            revision = environment["source_revision"]
            if result["source_revision"] is None:
                result["source_revision"] = revision
            if revision != result["source_revision"]:
                raise ValueError("Overview mixes measured source revisions")
            profiles[backend] = {"environment": environment, "sweep_environment": sweep}
            rows = read_csv(folder / "full.csv")
            overview[backend] = [selected_median(rows, scenario, count, mean) for scenario in SCENARIOS]
            for entry, (_, work, size, alignment, threads) in zip(entries, WORKLOADS):
                entry["values"][backend] = selected_median(rows, (work, size, alignment, threads), count, mean)
            rows = read_csv(folder / "pressure.csv")
            for path in ("native", "disabled"):
                for phase in PHASES:
                    selected = [row for row in rows if row["path"] == path and row["phase"] == phase]
                    if len(selected) != count or {row["trial"] for row in selected} != {str(trial) for trial in range(1, count + 1)}:
                        raise ValueError("Incomplete overview memory phase")
                    reducer = statistics.mean if mean else statistics.median
                    pressure.append({"backend": backend, "path": path, "phase": phase,
                        "requested_mib": reducer(int(row["requested_bytes"]) / 1048576 for row in selected),
                        "resident_delta_mib": reducer(
                            (int(row["rss"]) - int(row["baseline_rss"])) / 1048576 for row in selected)})
        result["platforms"][short] = {"workloads": entries, "pressure": pressure,
                                      "overview": overview, "profiles": profiles}
    result.update(measurement_schema=contract[0], trials=contract[1],
                  statistic="arithmetic mean" if contract[0] == 3 else "legacy median",
                  statistics={"aggregation": "mean" if contract[0] == 3 else "median", "trials": contract[1]})
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
        if LOCAL_MARKER in block:
            validate_local_overview(root, block)
            prepared.append((path, content))
            continue
        if "performance-overview-source:" in block:
            raise ValueError("Unknown README performance overview source")
        lines = block.splitlines()
        first = next(index for index, line in enumerate(lines) if line.startswith(header))
        if len(lines[first:first + 6]) != 6 or any(not line.startswith("|") for line in lines[first:first + 6]):
            raise ValueError("README performance table no longer matches four-row overview")
        for index in range(4):
            label = lines[first + 2 + index].split("|")[1].strip()
            values = " | ".join(f"{windows['overview'][backend][index]:.1f}" for backend in BACKENDS)
            lines[first + 2 + index] = f"| {label} | {values} |"
        for index, line in enumerate(lines):
            if line.startswith("Windows x64 \xb7"):
                line = re.sub(r"MSVC [\d./]+", compiler, line)
                line = re.sub(r"\d{4}-\d{2}-\d{2}", measured, line)
                if name == "README.md":
                    description = (f"Arithmetic mean of {data['trials']} fresh-process trials" if data["measurement_schema"] == 3 else
                                   f"Legacy median of {data['trials']} trials")
                    line = re.sub(r"(?:Legacy median|Median|Arithmetic mean) of (?:three|\d+)(?: fresh-process)? trials", description, line)
                else:
                    description = (f"\u53d6 {data['trials']} \u8f6e\u72ec\u7acb\u8fdb\u7a0b\u6d4b\u91cf\u7684\u7b97\u672f\u5e73\u5747\u503c" if data["measurement_schema"] == 3 else
                                   f"\u5386\u53f2 {data['trials']} \u8f6e\u6d4b\u91cf\u7684\u4e2d\u4f4d\u6570")
                    line = re.sub(r"\u53d6\u4e09\u6b21\u6d4b\u91cf\u7684\u4e2d\u4f4d\u6570|\u5386\u53f2 \d+ \u8f6e\u6d4b\u91cf\u7684\u4e2d\u4f4d\u6570|\u53d6 \d+ \u8f6e\u72ec\u7acb\u8fdb\u7a0b\u6d4b\u91cf\u7684\u7b97\u672f\u5e73\u5747\u503c", description, line)
                lines[index] = line
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
    parser.add_argument("--run-id")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.artifacts:
        if not args.source_revision:
            parser.error("--artifacts requires --source-revision")
        stage_artifacts(root, args.artifacts.resolve(), args.source_revision, args.run_id)
    data = refresh_overview(root, args.output.resolve(), args.source_revision)
    if args.native_input:
        load_tool(root, "refresh-performance-tables").refresh_tables(root, args.native_input.resolve())
    print("Updated overview from measured source", data["source_revision"])


if __name__ == "__main__":
    main()
