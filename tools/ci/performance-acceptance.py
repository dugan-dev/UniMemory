"""Fail each matched Native/API scenario below the configured performance ratio."""
import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
MAX_ITERATIONS = 200000000
sys.path.insert(0, str(ROOT / "tools"))
from benchmark_profile import read_profile, environment_for


def validate_profile(profile):
    if profile.get("statistics") != "OFF" or profile.get("configuration") != "Release" or profile.get("checks") not in ("AUTO", "OFF"):
        raise ValueError("Performance acceptance requires Release, statistics OFF and checks AUTO/OFF")
    parameters = profile.get("build_parameters", {})
    if parameters.get("sanitizer") or parameters.get("coverage", "OFF") != "OFF":
        raise ValueError("Performance acceptance forbids sanitizer/coverage instrumentation")
    flags = " ".join(str(value) for key, value in parameters.items() if "flags" in key or "options" in key)
    if re.search(r"[/-]fsanitize|--coverage|-fprofile|-ftest-coverage|/PROFILE", flags, re.IGNORECASE):
        raise ValueError("Performance acceptance forbids instrumented compiler/linker flags")


def validate_compiled_profile(profile, record, compiled):
    if any(compiled.get(key) != value for key, value in record.items()):
        raise ValueError("Compiled diagnostic profile differs from CMake; rebuild before measuring")
    expected = dict(actual_backend=profile["backend"], actual_statistics="OFF",
                    actual_checks=profile["checks"], actual_checks_enabled="0", actual_performance_tests="ON")
    if any(compiled.get(key) != value for key, value in expected.items()):
        raise ValueError("Executable was not compiled with the requested performance profile")


def compiled_profile(executable, profile):
    configuration = profile["configuration"]
    build = executable.parent.parent if executable.parent.name == configuration else executable.parent
    record = dict(line.split("=", 1) for line in
                  (build / f"diagnostic-build-{configuration}.txt").read_text(encoding="utf-8").splitlines() if "=" in line)
    process = subprocess.run([str(executable), "--profile"], text=True, capture_output=True, check=True,
                             timeout=30, env=environment_for(profile))
    compiled = dict(line.split("=", 1) for line in process.stdout.splitlines() if "=" in line)
    validate_compiled_profile(profile, record, compiled)
    return compiled


def affinity_for(system):
    return "single" if system in ("Windows", "Linux") else "none"


def retained_calibration(driver, executable, pair, target, raw, name, retain):
    iterations = 10000
    for pilot in range(1, 6):
        values = []
        for side in (0, 1) if pilot % 2 else (1, 0):
            identity = f"{name}-calibration-{pilot}-{pair[side]['variant']}"
            rows, stderr = driver.execute(executable, pair[side], iterations, raw, identity)
            value = driver.primary(rows)
            retain("calibration", pilot, side, iterations, identity, rows, stderr)
            values.append(value["seconds"])
        fastest = min(values)
        if fastest >= target:
            return iterations, pilot
        if iterations >= MAX_ITERATIONS or pilot == 5:
            raise ValueError("Calibration did not achieve the sustained interval; evidence retained")
        iterations = min(MAX_ITERATIONS, max(iterations + 1, math.ceil(iterations * target / max(fastest, 1e-6) * 1.15)))


def accepted(native, api, minimum):
    if len(native) != len(api) or len(native) < 10 or not .9 <= minimum <= 1:
        raise ValueError("Acceptance requires ten or more equal paired trials and a valid threshold")
    if any(not math.isfinite(value) or value <= 0 for value in (*native, *api)):
        raise ValueError("Every retained timing must be finite and positive")
    ratios = [left / right for left, right in zip(native, api)]
    return statistics.median(ratios) >= minimum and statistics.fmean(native) / statistics.fmean(api) >= minimum


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--minimum", type=float, default=.9)
    parser.add_argument("--trials", type=int, default=10)
    parser.add_argument("--seconds", type=float, default=.25)
    args = parser.parse_args()
    if args.trials < 10 or not .9 <= args.minimum <= 1 or not math.isfinite(args.seconds) or args.seconds < .125:
        parser.error("Use at least ten trials, a 0.90..1.00 minimum ratio and sustained timing intervals")
    executable = args.executable.resolve()
    profile = read_profile(executable)
    validate_profile(profile)
    if profile["build_parameters"].get("performance_tests") != "ON":
        raise ValueError("Build with UNIMEMORY_PERFORMANCE_TESTS=ON to enable the verified test configuration")
    os.environ.update(environment_for(profile))
    compiled = compiled_profile(executable, profile)
    validate_profile(dict(profile, build_parameters=compiled))
    spec = importlib.util.spec_from_file_location("diagnosis", ROOT / "tools/ci/diagnose-performance.py")
    driver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(driver)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Use a fresh output directory; retained samples must never be overwritten")
    raw = output / "raw"
    raw.mkdir()
    calibration_raw = output / "calibration-raw"
    calibration_raw.mkdir()
    affinity = affinity_for(platform.system())
    executable_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    manifest = dict(schema=2, profile=profile, compiled_profile=compiled, executable_sha256=executable_hash,
                    trials=args.trials, minimum_ratio=args.minimum, target_seconds=args.seconds,
                    max_iterations=MAX_ITERATIONS,
                    platform=platform.platform(), affinity=affinity, affinity_supported=affinity == "single",
                    affinity_note="Process pinned to first allowed CPU" if affinity == "single" else "Unpinned; OS CPU affinity unsupported",
                    phases=["calibration", "measurement"], calibration_sample_count=0, measurement_sample_count=0,
                    completed=False,
                    ordering="Alternating Native/API in independent processes; all samples retained",
                    aggregation="Per-scenario paired median AND ratio of arithmetic mean durations")
    (output / "environment.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    backend = profile["backend"]
    pairs = [("raw:" + str(size), driver.specimen("hotpath", backend, "native", size=size, affinity=affinity),
              driver.specimen("hotpath", backend, "api_cached", size=size, affinity=affinity))
             for size in (16, 64, 256, 4096, 65536)]
    pairs += [(name, driver.specimen("api", backend, "native_" + name, affinity=affinity),
               driver.specimen("api", backend, "api_" + name, affinity=affinity)) for name in driver.API]
    summaries = []
    samples = []
    for name, native, api in pairs:
        def retain(phase, trial, side, iterations, identity, rows, stderr):
            samples.extend(dict(phase=phase, scenario=name, trial=trial, side=side,
                                iterations=iterations, raw_identity=identity, affinity=affinity, **value) for value in rows)
            manifest["calibration_sample_count" if phase == "calibration" else "measurement_sample_count"] += 1
            driver.write_csv(output / "samples.csv", samples)
            (output / "environment.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        count, pilots = retained_calibration(driver, executable, (native, api), args.seconds,
                                             calibration_raw, name.replace(':', '-'), retain)
        controls, changes = [], []
        for trial in range(args.trials):
            for case in (native, api) if trial % 2 == 0 else (api, native):
                identity = f"{name.replace(':', '-')}-measurement-{trial + 1}-{case['variant']}"
                rows, stderr = driver.execute(executable, case, count, raw, identity)
                retain("measurement", trial + 1, 0 if case is native else 1, count, identity, rows, stderr)
                (controls if case is native else changes).append(driver.primary(rows))
        row = driver.summary_row(name, "acceptance", (native, api), controls, changes)
        row.update(phase="measurement", calibration_pilots=pilots, calibration_sample_count=pilots * 2)
        native_times = [value["value"] for value in controls]
        api_times = [value["value"] for value in changes]
        row["mean_performance_percent"] = statistics.fmean(native_times) / statistics.fmean(api_times) * 100
        row["passed"] = accepted(native_times, api_times, args.minimum)
        summaries.append(row)
        driver.write_csv(output / "summary.csv", summaries)
        print(f"{name}: median={row['performance_percent']:.2f}% mean={row['mean_performance_percent']:.2f}% {'PASS' if row['passed'] else 'FAIL'}", flush=True)
    if (hashlib.sha256(executable.read_bytes()).hexdigest() != executable_hash or read_profile(executable) != profile
            or compiled_profile(executable, profile) != compiled):
        raise RuntimeError("Executable or SDK profile changed during measurement")
    manifest.update(completed=True, passed=all(row["passed"] for row in summaries))
    (output / "environment.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if any(not row["passed"] for row in summaries):
        raise SystemExit("Performance acceptance failed; retain the evidence and fix every slow scenario")


if __name__ == "__main__":
    main()
