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
    if not manifest.get("source_revision") or manifest["source_revision"] == "unrecorded":
        raise ValueError("Missing source provenance")
    for file in ("latency.csv", "pressure.csv", "footprint.csv", "heap.csv"):
        if not read(directory / file):
            raise ValueError(f"Empty {file}")
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
    for tick in range(6):
        y = top + plot_height * (1 - tick / 5)
        parts.extend([f'<path d="M{left},{y}h{plot_width}" stroke="#e2e8f0"/>',
                      f'<text x="80" y="{y+5}" text-anchor="end" font-size="13">{maximum*tick/5:.2g}</text>'])
    for index, value in enumerate(xs):
        x = left + plot_width * (index + .5) / len(xs)
        parts.append(f'<text x="{x}" y="465" text-anchor="middle" font-size="14">{html.escape(str(value))}</text>')
    for number, (backend, path, values) in enumerate(series):
        color = COLORS[backend]
        points = []
        for index, value in enumerate(values):
            x = left + plot_width * (index + .5) / len(xs)
            y = top + plot_height * (1 - value / maximum)
            if bars:
                bar_width = plot_width / len(xs) / (len(series) + 2)
                x += (number - (len(series)-1)/2) * bar_width
                parts.append(f'<rect x="{x-bar_width*.42}" y="{y}" width="{bar_width*.84}" height="{top+plot_height-y}" fill="{color}" opacity="{.45 if path == "native" else .9}"/>')
            else:
                points.append(f"{x},{y}")
                parts.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{color}"/>')
        if not bars:
            dash = ' stroke-dasharray="7 5"' if path == "native" else ''
            parts.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2.5"{dash}/>')
        label = f"{backend} / {'Native' if path == 'native' else 'UniMemory'}"
        parts.extend([f'<path d="M880,{115+number*34}h28" stroke="{color}" stroke-width="3"'+ (' stroke-dasharray="7 5"' if path == 'native' else '') + '/>',
                      f'<text x="880" y="{133+number*34}" font-size="12">{label}</text>'])
    parts.extend([f'<text x="92" y="505" font-size="13" fill="#475569">{html.escape(caption[:145])}</text>',
                  '<text x="92" y="528" font-size="12" fill="#64748b">Three fresh-process trials; median shown. Native/API matched; statistics disabled. See raw results.</text>', '</g></svg>'])
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
    summary = {"source_revision": manifest["source_revision"], "run_url": manifest["run_url"],
               "scope": args.label, "logical_cpus": manifest["logical_cpus"],
               "oversubscribed_threads": [threads for threads in THREADS if threads > manifest["logical_cpus"]],
               "performance_gate": manifest["performance_gate"], "tail_method": manifest["tail_method"]}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
