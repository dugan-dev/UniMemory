"""Refresh detailed documentation tables from retained measurements, offline.

Only marked table/source blocks are generated. An optional native input must be
the native-application-comparison artifact from the same run as the SDK data.
Previously retained native results keep their own provenance when omitted.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import re
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from publication_metadata import sanitize_publication

PLATFORMS = (("windows-x64", "Windows"), ("linux-x64", "Linux"),
             ("macos-arm64", "macOS"))
BACKENDS = ("standard", "mimalloc", "jemalloc")
BACKEND_LABELS = {"standard": "Standard", "mimalloc": "mimalloc", "jemalloc": "jemalloc"}
PHASES = ("dense", "sparse", "churn_dense", "churn_sparse", "freed")
WORKLOADS = (("object", 64, 1, 1, "make_unique, 64 B"),
             ("shared_object", 64, 1, 1, "make_shared, 64 B"),
             ("std_vector", 256, 8, 1, "vector, 32 uint64_t"),
             ("pmr_vector", 256, 8, 1, "PMR vector, 32 uint64_t"),
             ("zeroed", 4096, 16, 1, "Zeroed Block, 4 KiB"),
             ("reallocate", 4096, 16, 1, "Block resize, 4 to 8 KiB"),
             ("mixed_lifetime", 4096, 16, 1, "Mixed lifetimes, 4-16 KiB"),
             ("cross_thread", 64, 16, 8, "Cross-thread free, 64 B, 8 threads"))
WORKLOAD_LABELS_ZH = {
    "make_unique, 64 B": "make_unique\uff0c64 B",
    "make_shared, 64 B": "make_shared\uff0c64 B",
    "vector, 32 uint64_t": "vector\uff0c32 \u4e2a uint64_t",
    "PMR vector, 32 uint64_t": "PMR vector\uff0c32 \u4e2a uint64_t",
    "Zeroed Block, 4 KiB": "\u96f6\u521d\u59cb\u5316 Block\uff0c4 KiB",
    "Block resize, 4 to 8 KiB": "Block \u6269\u5bb9\uff0c4 \u2192 8 KiB",
    "Mixed lifetimes, 4-16 KiB": "\u6df7\u5408\u751f\u547d\u5468\u671f\uff0c4\u201316 KiB",
    "Cross-thread free, 64 B, 8 threads": "\u8de8\u7ebf\u7a0b\u91ca\u653e\uff0c64 B\uff0c8 \u7ebf\u7a0b",
}
PROGRAMS = ("cfrac", "espresso", "barnes", "larson", "larson-sized",
            "alloc-test", "cache-scratch", "cache-thrash", "xmalloc-test",
            "malloc-large-old", "malloc-large", "mstress", "mleak", "rptest",
            "glibc-simple", "glibc-thread")
FIXED_DURATION = {"larson", "larson-sized", "xmalloc-test"}
MIB = 1024 ** 2


def read_csv(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def median_trial(rows, value):
    """Reduce exactly one finite result from each of three process trials."""
    if len(rows) != 3 or {row["trial"] for row in rows} != {"1", "2", "3"}:
        raise ValueError("Expected exactly one measurement from each of three trials")
    values = [float(value(row)) for row in rows]
    if any(not math.isfinite(item) for item in values):
        raise ValueError("Non-finite measurement")
    return statistics.median(values)


def select(rows, **dimensions):
    return [row for row in rows if all(row.get(name) == str(value)
                                     for name, value in dimensions.items())]


def metric(rows, field, **dimensions):
    return median_trial(select(rows, **dimensions), lambda row: row[field])


def resident_delta(rows, field, **dimensions):
    return median_trial(select(rows, **dimensions),
                        lambda row: (float(row[field]) - float(row["baseline_rss"])) / MIB)


def native_summary(rows):
    required = {"trial", "backend", "workload", "elapsed_seconds", "user_seconds",
                "system_seconds", "peak_rss_kib", "exit_code"}
    if any(not required <= row.keys() for row in rows):
        raise ValueError("Missing native application result fields")
    expected = {(str(trial), backend, program) for trial in range(1, 4)
                for backend in BACKENDS for program in PROGRAMS}
    actual = [(row["trial"], row["backend"], row["workload"]) for row in rows]
    if len(actual) != 144 or len(set(actual)) != 144 or set(actual) != expected:
        raise ValueError("Expected 144 unique native runs across 16 programs")
    if any(row["exit_code"] != "0" for row in rows):
        raise ValueError("Native application run failed")
    if any(not math.isfinite(float(row[field])) or float(row[field]) < 0
           for row in rows for field in ("elapsed_seconds", "user_seconds", "system_seconds", "peak_rss_kib")):
        raise ValueError("Invalid native application measurement")
    return {
        (program, backend): (
            metric(rows, "elapsed_seconds", workload=program, backend=backend),
            metric(rows, "peak_rss_kib", workload=program, backend=backend) / 1024)
        for program in PROGRAMS for backend in BACKENDS
    }


def validate_native_data(directory):
    directory = Path(directory)
    native_summary(read_csv(directory / "results.csv"))
    manifest = json.loads((directory / "environment.json").read_text(encoding="utf-8"))
    if (manifest.get("schema") != 1 or manifest.get("trials") != 3 or
            manifest.get("threads") != 4 or
            manifest.get("interface") != "native malloc/new through LD_PRELOAD" or
            set(manifest.get("commands", {})) != set(PROGRAMS) or
            not re.fullmatch(r"[0-9a-f]{40}", manifest.get("source_revision", "")) or
            not re.fullmatch(r"[0-9a-f]{40}", manifest.get("revision", "")) or
            not re.fullmatch(r"[0-9]+", str(manifest.get("run_id", ""))) or
            not re.fullmatch(r"\d{4}-\d{2}-\d{2}", manifest.get("measured_at_utc", "")) or
            not manifest.get("run_url", "").endswith("/actions/runs/" + str(manifest.get("run_id", "")))):
        raise ValueError("Invalid native application provenance or scope")
    return manifest


def sdk_inputs(root):
    datasets = {}
    manifests = []
    for platform, _ in PLATFORMS:
        for backend in BACKENDS:
            for mode in ("OFF", "ON"):
                directory = root / "docs/results/current" / platform / backend / mode
                manifest = json.loads((directory / "environment.json").read_text(encoding="utf-8"))
                profile = manifest.get("build_profile", {})
                if (manifest.get("schema") != 2 or manifest.get("trials") != 3 or
                        profile.get("backend") != backend or profile.get("statistics") != mode):
                    raise ValueError("Unexpected performance dataset profile")
                manifests.append(manifest)
                datasets[platform, backend, mode] = {
                    name: read_csv(directory / f"{name}.csv")
                    for name in ("full", "latency", "pressure", "footprint")
                }
    revisions = {item["source_revision"] for item in manifests}
    runs = {str(item["run_id"]) for item in manifests}
    dates = {item["measured_at_utc"][:10] for item in manifests}
    if len(revisions) != 1 or len(runs) != 1 or len(dates) != 1:
        raise ValueError("Detailed tables require one consistent source revision and run")
    return datasets, manifests


def sdk_tables(datasets, manifests):
    tables = {}
    requested = []
    cross_thread = []
    stack = []
    footprint = []
    storage = []
    rounding = []
    for platform, label in PLATFORMS:
        for backend in BACKENDS:
            for path in ("native", "disabled"):
                requested.append(tuple(metric(datasets[platform, backend, 'OFF']['pressure'],
                                              'requested_bytes', backend=backend, path=path, phase=phase) / MIB
                                       for phase in PHASES))
        tables[f"workloads-{platform}"] = [
            [title, *[f"{metric(datasets[platform, backend, 'OFF']['full'], 'median_ns_per_operation', backend=backend, workload=workload, bytes=size, alignment=alignment, threads=threads, statistics='disabled', heap='no'):.1f}"
                      for backend in BACKENDS]]
            for workload, size, alignment, threads, title in WORKLOADS
        ]
        tables[f"latency-{platform}"] = [
            [str(size), BACKEND_LABELS[backend],
             f"{metric(datasets[platform, backend, 'OFF']['latency'], 'median_ns', backend=backend, path='native', bytes=size):.1f}",
             f"{metric(datasets[platform, backend, 'OFF']['latency'], 'median_ns', backend=backend, path='disabled', bytes=size):.1f}",
             f"{metric(datasets[platform, backend, 'ON']['latency'], 'median_ns', backend=backend, path='basic', bytes=size):.1f}"]
            for size in (16, 64, 256, 4096, 65536) for backend in BACKENDS
        ]
        tables[f"pressure-{platform}"] = [
            [BACKEND_LABELS[backend], "Native" if path == "native" else "UniMemory",
             *[f"{resident_delta(datasets[platform, backend, 'OFF']['pressure'], 'rss', backend=backend, path=path, phase=phase):.2f}"
               for phase in PHASES]]
            for backend in BACKENDS for path in ("native", "disabled")
        ]
        for backend in BACKENDS:
            data = datasets[platform, backend, "OFF"]
            for heap in (("no",) if backend == "standard" else ("no", "yes")):
                dimensions = dict(backend=backend, statistics="disabled", heap=heap)
                footprint.append([label, BACKEND_LABELS[backend], "Heap" if heap == "yes" else "Global",
                                  *[f"{resident_delta(data['footprint'], field, **dimensions):.2f}"
                                    for field in ("live_rss", "freed_rss")],
                                  f"{resident_delta(data['footprint'], 'collected_rss', **dimensions):.2f}" if heap == "yes" else "\u2014"])
            for path in ("native", "disabled"):
                samples = select(data["pressure"], backend=backend, path=path, phase="dense")
                if backend == "standard" or any(not row.get("usable_bytes") for row in samples):
                    overhead = "\u2014"
                else:
                    overhead = f"{median_trial(samples, lambda row: (float(row['usable_bytes']) / float(row['requested_bytes']) - 1) * 100):.2f}%"
                rounding.append([label, BACKEND_LABELS[backend], "Native" if path == "native" else "UniMemory", overhead])
        for threads in (2, 4, 8):
            cross_thread.append([label, str(threads), *[
                f"{metric(datasets[platform, backend, 'OFF']['full'], 'median_ns_per_operation', backend=backend, workload='cross_thread', bytes=64, alignment=16, threads=threads, statistics='disabled', heap='no'):.1f}"
                for backend in BACKENDS]])
        for size in (64, 4096):
            stack.append([label, str(size), f"{metric(datasets[platform, 'standard', 'OFF']['full'], 'median_ns_per_operation', backend='fixed_buffer', workload='stack_mark_rewind', bytes=size, alignment=16, threads=1, statistics='disabled', heap='no'):.2f}"])
        # Manifests follow platform/backend/mode order; type sizes must agree
        # across all six build profiles on the platform.
        index = [item[0] for item in PLATFORMS].index(platform) * 6
        environments = [item["allocator_environment"] for item in manifests[index:index + 6]]
        values = [tuple(item[key] for key in ("memory_bytes", "owned_block_bytes", "allocator_bytes"))
                  for item in environments]
        if len(set(values)) != 1:
            raise ValueError("Type storage differs between backend/statistics profiles")
        storage.append([label, *map(str, values[0])])
    tables.update({"cross-thread": cross_thread, "stack": stack,
                   "footprint": footprint, "rounding": rounding, "storage": storage})
    if len(set(requested)) != 1:
        raise ValueError("Pressure workload requested sizes differ between native/API or platform runs")
    tables["pressure-requested"] = [[f"{item:.2f}" for item in requested[0]]]
    return tables


def source_line(manifest, native=False):
    revision = manifest["source_revision"]
    date = manifest["measured_at_utc"][:10]
    run_url = manifest["run_url"]
    raw = "../results/current/native-applications/README.md" if native else "../results/current/README.md"
    return f"{date} \u00b7 [{revision[:7]}]({run_url}) \u00b7 [CSV]({raw})"


def import_native(root, directory, sdk_manifest):
    rows = read_csv(directory / "results.csv")
    native_summary(rows)
    manifest = json.loads((directory / "environment.json").read_text(encoding="utf-8"))
    manifest.update({key: sdk_manifest[key] for key in ("source_revision", "run_id", "run_url")})
    manifest["schema"] = 1
    manifest["measured_at_utc"] = sdk_manifest["measured_at_utc"][:10]
    manifest["measurement_date_source"] = "workflow run date; native artifact has no per-process timestamp"
    manifest["artifact_name"] = "native-application-comparison"
    destination = root / "docs/results/current/native-applications"
    destination.mkdir(parents=True, exist_ok=True)
    with (destination / "results.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    (destination / "environment.json").write_text(
        json.dumps(sanitize_publication(manifest), indent=2) + "\n", encoding="utf-8", newline="\n")
    (destination / "README.md").write_text(
        "# Native application results\n\n"
        f"Run date: {manifest['measured_at_utc']} \u00b7 [workflow run]({manifest['run_url']}) "
        f"\u00b7 UniMemory workflow source `{manifest['source_revision'][:7]}`.\n\n"
        "[results.csv](results.csv) retains all 144 successful process runs: 16 programs, "
        "three native allocators and three trials. "
        "[environment.json](environment.json) records commands and the pinned mimalloc-bench revision.\n\n"
        "These programs use native malloc/new through LD_PRELOAD; they do not measure the UniMemory API. "
        "Elapsed seconds include startup. Peak RSS is recorded in KiB. "
        "Larson and xmalloc-test run for a configured duration; elapsed time is not a speed ranking.\n",
        encoding="utf-8", newline="\n")
    validate_native_data(destination)


def localized_rows(name, rows, language):
    translated = [list(row) for row in rows]
    if language == "en":
        return translated
    if language != "zh-CN":
        raise ValueError("Unsupported performance table language")
    for row in translated:
        if name.startswith("workloads-"):
            row[0] = WORKLOAD_LABELS_ZH[row[0]]
        elif name.startswith("pressure-") and name != "pressure-requested":
            if row[1] == "Native":
                row[1] = "\u539f\u751f"
        elif name == "rounding":
            if row[2] == "Native":
                row[2] = "\u539f\u751f"
        elif name == "footprint":
            row[2] = {"Global": "Global\uff08\u5168\u5c40\uff09",
                      "Heap": "Heap\uff08\u72ec\u7acb\uff09"}[row[2]]
    return translated


def replace_blocks(content, tables, source, expected=None, language="en"):
    seen = set()
    pattern = r"<!-- performance-table:([a-z0-9-]+) -->\n(.*?)\n<!-- /performance-table -->"

    def replace(match):
        name, old = match.groups()
        if name in seen or name not in tables:
            raise ValueError(f"Unexpected or duplicate performance table: {name}")
        seen.add(name)
        header = old.splitlines()[0]
        columns = [item.strip() for item in header.strip("|").split("|")]
        rows = localized_rows(name, tables[name], language)
        if any(len(row) != len(columns) for row in rows):
            raise ValueError(f"Table column mismatch: {name}")
        body = "\n".join([header, "| " + " | ".join("---" for _ in columns) + " |",
                          *["| " + " | ".join(row) + " |" for row in rows]])
        return f"<!-- performance-table:{name} -->\n{body}\n<!-- /performance-table -->"

    updated = re.sub(pattern, replace, content, flags=re.DOTALL)
    if not seen:
        raise ValueError("No generated performance table blocks found")
    if expected is not None and seen != set(expected):
        raise ValueError("Missing or unexpected detailed performance table blocks")
    source_pattern = r"<!-- performance-source -->\n.*?\n<!-- /performance-source -->"
    if len(re.findall(source_pattern, updated, flags=re.DOTALL)) != 1:
        raise ValueError("Expected one performance source block")
    return re.sub(source_pattern, f"<!-- performance-source -->\n{source}\n<!-- /performance-source -->",
                  updated, flags=re.DOTALL)


def refresh_tables(root, native_input=None):
    root = Path(root)
    datasets, manifests = sdk_inputs(root)
    tables = sdk_tables(datasets, manifests)
    if native_input is not None:
        import_native(root, Path(native_input), manifests[0])
    native_directory = root / "docs/results/current/native-applications"
    native = native_summary(read_csv(native_directory / "results.csv"))
    native_manifest = validate_native_data(native_directory)
    tables["applications-elapsed"] = [
        [program, *[("\u2014" if program in FIXED_DURATION else f"{native[program, backend][0]:.3f}")
                    for backend in BACKENDS]] for program in PROGRAMS]
    tables["applications-memory"] = [
        [program, *[f"{native[program, backend][1]:.2f}" for backend in BACKENDS]]
        for program in PROGRAMS]
    changed = []
    expected = {
        "latency": {"cross-thread", "stack", *[f"{kind}-{platform}" for kind in ("workloads", "latency") for platform, _ in PLATFORMS]},
        "memory": {"pressure-requested", "rounding", "footprint", "storage", *[f"pressure-{platform}" for platform, _ in PLATFORMS]},
        "applications": {"applications-elapsed", "applications-memory"},
    }
    for name in ("latency", "memory", "applications"):
        for suffix in (".md", ".zh-CN.md"):
            path = root / "docs/performance" / (name + suffix)
            original = path.read_text(encoding="utf-8")
            updated = replace_blocks(original, tables,
                                     source_line(native_manifest if name == "applications" else manifests[0],
                                                 native=name == "applications"), expected=expected[name],
                                     language="zh-CN" if suffix == ".zh-CN.md" else "en")
            if updated != original:
                path.write_text(updated, encoding="utf-8", newline="\n")
                changed.append(path)
    return changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--native-input", type=Path,
                        help="native-application-comparison artifact from the SDK measurement run")
    args = parser.parse_args()
    changed = refresh_tables(args.root, args.native_input)
    print(f"Refreshed {len(changed)} detailed performance pages")


if __name__ == "__main__":
    main()
