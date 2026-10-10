"""Measure matched native/API workloads in fresh processes; retain raw samples."""
import argparse
import csv
import gzip
import hashlib
import io
import json
import math
import os
from pathlib import Path
import platform
import random
import re
import statistics
import subprocess
import sys

BACKENDS = ("standard", "mimalloc", "jemalloc")
PATHS = ("native", "disabled", "basic")
SIZES = (16, 64, 256, 4096, 65536)
THREADS = (1, 2, 4, 8, 16)
TRIALS = 3


def percentile(values, fraction):
    if not values or not 0 < fraction <= 1:
        raise ValueError("Invalid percentile input")
    return sorted(values)[math.ceil(len(values) * fraction) - 1]


def write_rows(path, rows):
    if not rows:
        raise ValueError("Empty measurements")
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def measure(executable, backend, workload, path, *parameters):
    environment = os.environ.copy()
    environment["MIMALLOC_DISABLE_REDIRECT"] = "1"
    result = subprocess.run([str(executable), backend, workload, path, *map(str, parameters)],
                            env=environment, capture_output=True, text=True, check=True, timeout=180)
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    if not rows:
        raise RuntimeError("Benchmark produced no records")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[2]
    subprocess.run([sys.executable, str(root / "tools/run-benchmarks.py"), str(executable),
                    str(args.output)], check=True)
    raw = args.output / "raw"
    raw.mkdir(exist_ok=True)
    scale_cases = [(backend, path, workload, size, threads) for backend in BACKENDS
                   for path in PATHS for workload in ("scaling", "handoff")
                   for size in (64, 4096, 65536) for threads in THREADS]
    random.Random(0x20261010).shuffle(scale_cases)
    scaling = []
    tails = []
    for trial in range(1, TRIALS + 1):
        for backend, path, workload, size, threads in scale_cases:
            rows = measure(executable, backend, workload, path, size, threads)
            if len(rows) != 1:
                raise RuntimeError("Unexpected scaling record count")
            scaling.append({"trial": trial, **rows[0]})
        tail_cases = [(backend, path, size) for backend in BACKENDS for path in PATHS for size in SIZES]
        random.Random(trial).shuffle(tail_cases)
        for backend, path, size in tail_cases:
            rows = measure(executable, backend, "tails", path, size)
            if len(rows) != 8192:
                raise RuntimeError("Incomplete individual-operation samples")
            buffer = io.StringIO()
            writer = csv.DictWriter(buffer, list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
            (raw / f"{trial}-{backend}-{path}-{size}.csv.gz").write_bytes(
                gzip.compress(buffer.getvalue().encode(), mtime=0))
            allocation = [float(row["allocate_ns"]) for row in rows]
            free = [float(row["free_ns"]) for row in rows]
            tails.append({"trial": trial, "backend": backend, "path": path, "bytes": size,
                          "samples": len(rows), "allocate_p50_ns": percentile(allocation, .5),
                          "allocate_p95_ns": percentile(allocation, .95),
                          "allocate_p99_ns": percentile(allocation, .99),
                          "allocate_p999_ns": percentile(allocation, .999),
                          "allocate_max_ns": max(allocation), "free_p99_ns": percentile(free, .99),
                          "clock_p50_ns": statistics.median(float(row["clock_ns"]) for row in rows)})
        print(f"Completed performance trial {trial}/{TRIALS}", flush=True)
    write_rows(args.output / "scaling.csv", scaling)
    write_rows(args.output / "tails.csv", tails)
    manifest = json.loads((args.output / "environment.json").read_text())
    compiler_files = list(executable.parent.parent.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"))
    if not compiler_files:
        compiler_files = list(executable.parent.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"))
    if len(compiler_files) != 1:
        raise RuntimeError("Missing benchmark compiler provenance")
    compiler_text = compiler_files[0].read_text()
    compiler = dict(re.findall(r'set\((CMAKE_CXX_(?:COMPILER_ID|COMPILER_VERSION)) "([^"\n]+)"\)', compiler_text))
    cpu = platform.processor()
    if platform.system() == "Linux":
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    elif platform.system() == "Darwin":
        cpu = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
    manifest.update({"schema": 1, "source_revision": os.environ.get("GITHUB_SHA", "unrecorded"),
                     "run_id": os.environ.get("GITHUB_RUN_ID", "unrecorded"),
                     "run_url": f"https://github.com/{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/{os.environ.get('GITHUB_RUN_ID', '')}",
                     "architecture": platform.machine(), "trials": TRIALS,
                     "compiler": compiler, "cpu_model": cpu, "measurement_protocol": "native-api-scaling-tails-v1",
                     "threads": list(THREADS), "sizes": list(SIZES), "backends": list(BACKENDS),
                     "paths": list(PATHS), "tail_samples": 8192,
                     "tail_method": "Individual allocate/free intervals; includes clock overhead, separately recorded",
                     "scaling_method": "4096 allocation/free pairs per thread, synchronized batches; thread creation excluded, join included",
                     "performance_gate": "Report only; hosted hardware noise is not a correctness failure",
                     "benchmark_sha256": hashlib.sha256((root / "benchmarks/delivery_bench.cpp").read_bytes()).hexdigest()})
    (args.output / "environment.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
