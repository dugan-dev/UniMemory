"""Validate complete measurements and render publication SVGs without dependencies."""
import argparse
import csv
from collections import defaultdict
import html
import itertools
import json
import math
from pathlib import Path
import statistics

BACKENDS = ("standard", "mimalloc", "jemalloc")
PATHS = ("native", "disabled", "basic")
THREADS = (1, 2, 4, 8, 16)
COLORS = {"standard": "#64748b", "mimalloc": "#0284c7", "jemalloc": "#d97706"}
API_SCENARIOS = (("raw", "64", "16"), ("zeroed", "64", "16"), ("reallocate", "64", "16"),
                 ("object", "64", "1"), ("shared_object", "64", "1"), ("std_vector", "256", "8"))


def sweep_scenarios():
    """Pinned full-sweep contract, independent of measurements and their manifest."""
    expected = set()
    for backend in BACKENDS:
        heaps = ("no",) if backend == "standard" else ("no", "yes")
        for heap, tracking in itertools.product(heaps, ("disabled", "basic")):
            for workload, size, alignment in itertools.product(
                    ("raw", "zeroed", "reallocate", "reallocate_zeroed", "batch", "mixed_lifetime", "owned_block"),
                    (64,256,4096,65536), (16,64,256)):
                expected.add((backend, workload, str(size), str(alignment), "1", tracking, heap))
            for workload, size in itertools.product(("typed_raw", "array"), (64,256,4096,65536)):
                expected.add((backend, workload, str(size), "1", "1", tracking, heap))
            for workload in ("create_destroy", "object", "shared_object"):
                expected.add((backend, workload, "64", "1", "1", tracking, heap))
            for workload in ("std_vector", "pmr_vector"):
                expected.add((backend, workload, "256", "8", "1", tracking, heap))
        for heap, threads in itertools.product(heaps, (2,4,8)):
            expected.add((backend, "cross_thread", "64", "16", str(threads), "disabled", heap))
        if backend != "standard":
            for heap in heaps:
                expected.add((backend, "detailed_statistics", "0", "0", "1", "disabled", heap))
    for size, alignment in itertools.product((64,256,4096,65536), (16,64,256)):
        expected.add(("fixed_buffer", "stack_mark_rewind", str(size), str(alignment), "1", "disabled", "no"))
    return expected


def read(path):
    with path.open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))


def validate(directory):
    manifest = json.loads((directory / "environment.json").read_text())
    if manifest.get("schema") != 1 or manifest.get("trials") != 3:
        raise ValueError("Unsupported or incomplete performance manifest")
    for name, expected, key in (
        ("scaling", set(itertools.product(range(1, 4), BACKENDS, PATHS, ("same_thread", "handoff"), (64, 4096, 65536), THREADS)),
         lambda row: (int(row["trial"]), row["backend"], row["path"], row["workload"], int(row["bytes"]), int(row["threads"]))),
        ("tails", set(itertools.product(range(1, 4), BACKENDS, PATHS, (16, 64, 256, 4096, 65536))),
         lambda row: (int(row["trial"]), row["backend"], row["path"], int(row["bytes"]))),
    ):
        rows = read(directory / f"{name}.csv")
        keys = [key(row) for row in rows]
        if len(keys) != len(expected) or set(keys) != expected:
            raise ValueError(f"Missing, duplicate or unexpected {name} scenarios")
        metric = "operations_per_second" if name == "scaling" else "allocate_p99_ns"
        if any(not math.isfinite(float(row[metric])) or float(row[metric]) <= 0 for row in rows):
            raise ValueError(f"Invalid {name} measurement")
        if name == "tails" and any(int(row["samples"]) != 8192 for row in rows):
            raise ValueError("Incomplete operation samples")
        numeric = ("operations", "seconds", "baseline_rss", "final_rss", "peak_rss") if name == "scaling" else (
            "allocate_p50_ns", "allocate_p95_ns", "allocate_p999_ns", "allocate_max_ns", "free_p99_ns", "clock_p50_ns")
        try:
            if any(not math.isfinite(float(row[field])) or float(row[field]) < 0 for row in rows for field in numeric):
                raise ValueError(f"Invalid {name} numeric field")
            if name == "scaling" and any(int(row["operations"]) != int(row["threads"])*4096 or
                                          float(row["seconds"]) <= 0 or float(row["peak_rss"]) <= 0 for row in rows):
                raise ValueError("Invalid scaling operation count or duration")
            if name == "tails" and any(not (float(row["allocate_p50_ns"]) <= float(row["allocate_p95_ns"]) <=
                                            float(row["allocate_p99_ns"]) <= float(row["allocate_p999_ns"]) <=
                                            float(row["allocate_max_ns"])) for row in rows):
                raise ValueError("Invalid percentile ordering")
        except KeyError as error:
            raise ValueError(f"Invalid {name} schema") from error
    if not manifest.get("source_revision") or manifest["source_revision"] == "unrecorded":
        raise ValueError("Missing source provenance")
    matrices = (
        ("latency", set(itertools.product(range(1, 4), BACKENDS, PATHS, (16,64,256,4096,65536))),
         ("trial", "backend", "path", "bytes"), ("min_ns", "median_ns", "max_ns")),
        ("pressure", set(itertools.product(range(1, 4), BACKENDS, PATHS,
                                           ("dense", "sparse", "churn_dense", "churn_sparse", "freed"))),
         ("trial", "backend", "path", "phase"), ("rss", "peak_rss", "requested_bytes", "live_blocks")),
        ("footprint", {(trial, backend, stats, heap) for trial in range(1,4) for backend in BACKENDS
                       for stats in ("disabled", "basic") for heap in (("no",) if backend == "standard" else ("no", "yes"))},
         ("trial", "backend", "statistics", "heap"), ("baseline_rss", "live_rss", "freed_rss", "collected_rss", "peak_rss")),
        ("heap", set(itertools.product(range(1,4), ("mimalloc", "jemalloc"), ("owns", "collect", "reset"))),
         ("trial", "backend", "operation"), ("median_ns", "operations")),
    )
    for name, expected, fields, metrics in matrices:
        rows = read(directory / f"{name}.csv")
        try:
            keys = [tuple(int(row[field]) if field in ("trial", "bytes") else row[field] for field in fields) for row in rows]
            if len(keys) != len(expected) or set(keys) != expected:
                raise ValueError(f"Missing, duplicate or unexpected {name} scenarios")
            if any(not math.isfinite(float(row[field])) or float(row[field]) < 0 for row in rows for field in metrics):
                raise ValueError(f"Invalid {name} measurement")
            if name == "pressure" and any(row["phase"] == "freed" and (int(row["live_blocks"]) or int(row["requested_bytes"])) for row in rows):
                raise ValueError("Pressure workload retained outstanding requests")
        except KeyError as error:
            raise ValueError(f"Invalid {name} schema") from error
    sweep = read(directory / "full.csv")
    sweep_manifest = json.loads((directory / "sweep-environment.json").read_text())
    scenario_fields = ("backend", "workload", "bytes", "alignment", "threads", "statistics", "heap")
    trials = {trial: [] for trial in range(1,4)}
    for row in sweep:
        trial = int(row["trial"])
        if trial not in trials or not math.isfinite(float(row["median_ns_per_operation"])) or float(row["median_ns_per_operation"]) <= 0:
            raise ValueError("Invalid full-sweep measurement")
        trials[trial].append(tuple(row[field] for field in scenario_fields))
    expected = sweep_scenarios()
    for keys in trials.values():
        if len(keys) != len(expected) or set(keys) != expected or len(keys) != sweep_manifest["scenarios_per_trial"]:
            raise ValueError("Incomplete full sweep")
    for backend, (workload, size, alignment) in itertools.product(BACKENDS, API_SCENARIOS):
        if (backend, workload, size, alignment, "1", "disabled", "no") not in expected:
            raise ValueError("Missing representative API workload")
    return manifest


def chart(title, ylabel, xs, series, caption, bars=False):
    width, height = 1100, 560
    left, top, plot_width, plot_height = 92, 92, 760, 350
    maximum = max(value for _, _, values in series for value in values) * 1.12
    if maximum <= 0:
        raise ValueError("Chart requires positive measurements")
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" role="img">',
             f'<title>{html.escape(title)}</title>', '<rect width="1100" height="560" fill="#ffffff"/>',
             '<g font-family="system-ui,Segoe UI,sans-serif" fill="#0f172a">',
             f'<text x="92" y="40" font-size="25" font-weight="650">{html.escape(title)}</text>',
             f'<text x="92" y="66" font-size="14" fill="#475569">{html.escape(ylabel)}</text>']
    if bars:
        parts.append('<defs>')
        for backend, color in COLORS.items():
            parts.append(f'<pattern id="{backend}-basic" width="6" height="6" patternUnits="userSpaceOnUse">'
                         f'<rect width="6" height="6" fill="{color}"/>'
                         '<path d="M0,6L6,0" stroke="#ffffff" stroke-opacity="0.6" stroke-width="1.2"/></pattern>')
        parts.append('</defs>')
    for tick in range(6):
        y = top + plot_height * (1 - tick / 5)
        parts.extend([f'<path d="M{left},{y}h{plot_width}" stroke="#e2e8f0"/>',
                      f'<text x="80" y="{y+5}" text-anchor="end" font-size="13">{maximum*tick/5:.2g}</text>'])
    for index, value in enumerate(xs):
        x = left + plot_width * (index + .5) / len(xs)
        parts.append(f'<text x="{x}" y="465" text-anchor="middle" font-size="14">{html.escape(str(value))}</text>')
    for number, (backend, path, values) in enumerate(series):
        color = COLORS[backend]
        fill = f'url(#{backend}-basic)' if path == "basic" else color
        opacity = .45 if path == "native" else .9
        points = []
        for index, value in enumerate(values):
            x = left + plot_width * (index + .5) / len(xs)
            y = top + plot_height * (1 - value / maximum)
            if bars:
                bar_width = plot_width / len(xs) / (len(series) + 2)
                x += (number - (len(series)-1)/2) * bar_width
                parts.append(f'<rect data-path="{path}" x="{x-bar_width*.42}" y="{y}" width="{bar_width*.84}" height="{top+plot_height-y}" fill="{fill}" opacity="{opacity}"/>')
            else:
                points.append(f"{x},{y}")
                parts.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{color}"/>')
        if not bars:
            dash = ' stroke-dasharray="7 5"' if path == "native" else ''
            parts.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2.5"{dash}/>')
        label = f"{backend} / {'Native' if path == 'native' else 'API + stats' if path == 'basic' else 'UniMemory'}"
        if bars:
            parts.append(f'<rect data-path="{path}" x="880" y="{110+number*34}" width="28" height="8" fill="{fill}" opacity="{opacity}"/>')
        else:
            parts.append(f'<path d="M880,{115+number*34}h28" stroke="{color}" stroke-width="3"'+ (' stroke-dasharray="7 5"' if path == 'native' else '') + '/>')
        parts.append(f'<text x="880" y="{133+number*34}" font-size="12">{label}</text>')
    parts.extend([f'<text x="92" y="505" font-size="13" fill="#475569">{html.escape(caption[:145])}</text>',
                  '<text x="92" y="528" font-size="12" fill="#64748b">Three fresh-process trials; median shown. See measurement method and raw data for scope and environment.</text>', '</g></svg>'])
    return "\n".join(parts) + "\n"


def aggregate(rows, key, metric):
    groups = defaultdict(list)
    for row in rows:
        groups[key(row)].append(float(row[metric]))
    return {key: statistics.median(values) for key, values in groups.items()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    manifest = validate(args.input)
    args.output.mkdir(parents=True, exist_ok=True)
    rows = read(args.input / "scaling.csv")
    series = [(backend, path) for backend in BACKENDS for path in ("native", "disabled")]
    caption = f"{args.label} · {manifest['logical_cpus']} logical CPUs · {manifest['source_revision'][:12]} · 64 B cross-thread workload"
    filtered = [row for row in rows if row["workload"] == "handoff" and int(row["bytes"]) == 64]
    for file, metric, title, ylabel, factor in (
        ("throughput", "operations_per_second", "Cross-thread throughput", "Million allocation/free pairs per second · higher is better", 1e6),
        ("memory", "peak_rss", "Peak resident memory", "MiB · lower is better", 1024**2),
    ):
        values = aggregate(filtered, lambda row: (row["backend"], row["path"], int(row["threads"])), metric)
        content = chart(title, ylabel, THREADS, [(backend, path, [values[backend, path, threads]/factor for threads in THREADS]) for backend, path in series], caption)
        (args.output / f"{file}.svg").write_text(content, encoding="utf-8")
    tail_values = aggregate(read(args.input / "tails.csv"), lambda row: (row["backend"], row["path"], int(row["bytes"])), "allocate_p99_ns")
    sizes = (16, 64, 256, 4096, 65536)
    (args.output / "latency.svg").write_text(chart("Individual allocation p99", "Nanoseconds · includes measured clock overhead · lower is better", sizes,
        [(backend, path, [tail_values[backend, path, size] for size in sizes]) for backend, path in series],
        f"{args.label} · {manifest['source_revision'][:12]} · individual operation samples, not batch averages", bars=True), encoding="utf-8")
    (args.output / "statistics.svg").write_text(chart("Statistics cost · allocation p99", "Nanoseconds · lower is better", sizes,
        [(backend, path, [tail_values[backend, path, size] for size in sizes]) for backend in BACKENDS for path in PATHS],
        f"{args.label} · {manifest['source_revision'][:12]} · Native, API, and API + statistics", bars=True), encoding="utf-8")
    phases = ("dense", "sparse", "churn_dense", "churn_sparse", "freed")
    memory = aggregate(read(args.input / "pressure.csv"), lambda row: (row["backend"], row["path"], row["phase"]), "rss")
    (args.output / "retention.svg").write_text(chart("Mixed-lifetime memory retention", "Resident MiB · not a fragmentation or leak rate", phases,
        [(backend, path, [memory[backend, path, phase]/1024**2 for phase in phases]) for backend, path in series],
        f"{args.label} · 16,384 slots; eight refill/free cycles; final phase has zero live requests"), encoding="utf-8")
    workloads = tuple(item[0] for item in API_SCENARIOS)
    sweep = [row for row in read(args.input / "full.csv") if (row["workload"], row["bytes"], row["alignment"]) in API_SCENARIOS and row["threads"] == "1" and
             row["statistics"] == "disabled" and row["heap"] == "no"]
    workload_values = aggregate(sweep, lambda row: (row["backend"], row["workload"]), "median_ns_per_operation")
    (args.output / "workloads.svg").write_text(chart("API workloads · normalized time", "Relative to UniMemory Standard = 1 · lower is better", workloads,
        [(backend, "disabled", [workload_values[backend, workload]/workload_values["standard", workload] for workload in workloads]) for backend in BACKENDS],
        f"{args.label} · API only, statistics disabled · workload operations differ; compare within each group", bars=True), encoding="utf-8")
    latency = aggregate(read(args.input / "latency.csv"), lambda row: (row["backend"], row["path"], int(row["bytes"])), "median_ns")
    analysis = [f"# Performance · {args.label}\n", f"Source: `{manifest['source_revision']}` · [remote run]({manifest['run_url']})\n",
                "Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.\n",
                "| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |\n| --- | ---: | ---: | ---: | ---: |"]
    for backend in BACKENDS:
        for size in sizes:
            native, api = latency[backend, "native", size], latency[backend, "disabled", size]
            analysis.append(f"| {backend} | {size} | {native:.2f} | {api:.2f} | {api/native:.3f} |")
    analysis.extend(["\n## Interpretation\n", "Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.\n"])
    previous = Path(__file__).resolve().parents[2] / "docs/results/current" / args.label
    previous_manifest = previous / "environment.json"
    if previous_manifest.exists():
        prior = json.loads(previous_manifest.read_text())
        comparable = all(prior.get(key) == manifest.get(key) for key in
                         ("compiler", "cpu_model", "logical_cpus", "measurement_protocol", "benchmark_sha256"))
        if comparable:
            old = aggregate(read(previous / "latency.csv"), lambda row: (row["backend"], row["path"], int(row["bytes"])), "median_ns")
            analysis.append("\n## Baseline signals\n\nSame recorded CPU/compiler/protocol. These signals require repeated confirmation.\n\n| Backend | Size | API/Native ratio change |\n| --- | ---: | ---: |")
            for backend in BACKENDS:
                for size in sizes:
                    before = old[backend, "disabled", size] / old[backend, "native", size]
                    now = latency[backend, "disabled", size] / latency[backend, "native", size]
                    analysis.append(f"| {backend} | {size} | {(now/before-1)*100:+.1f}% |")
        else:
            analysis.append("\nBaseline comparison withheld: recorded hardware, compiler or measurement protocol differs.\n")
    else:
        analysis.append("\nFirst recorded baseline; no regression comparison is available yet.\n")
    (args.output / "README.md").write_text("\n".join(analysis).rstrip() + "\n", encoding="utf-8")
    summary = {"source_revision": manifest["source_revision"], "run_url": manifest["run_url"],
               "scope": args.label, "logical_cpus": manifest["logical_cpus"],
               "oversubscribed_threads": [threads for threads in THREADS if threads > manifest["logical_cpus"]],
               "performance_gate": manifest["performance_gate"], "tail_method": manifest["tail_method"]}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
