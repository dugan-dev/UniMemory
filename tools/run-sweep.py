"""Measure unified API scenarios with common calibrated counts and retained trials."""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import subprocess
from datetime import datetime, timezone
from benchmark_profile import read_profile, environment_for
from benchmark_measurements import write_rows, TRIALS, PROTOCOL
from benchmark_calibration import semantic_hash, failure_record

FIELDS = ('backend', 'workload', 'bytes', 'alignment', 'threads', 'statistics', 'heap')


def calibration_key(row):
    return ':'.join(row[field] for field in ('workload', 'bytes', 'alignment', 'threads', 'heap'))


def calibrate_sweep(executable, output=None, effective_cpus=None):
    profile = read_profile(executable)
    command = [str(executable), '--full', '--calibrate', '--effective-cpus', str(effective_cpus or os.cpu_count() or 1)]
    try:
        result = subprocess.run(command, env=environment_for(profile), capture_output=True, text=True, check=True, timeout=600)
    except Exception as error:
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            (Path(output) / 'full-calibration-failures.json').write_text(json.dumps(
                failure_record(error, kind='full-calibration'), indent=2) + '\n', encoding='utf-8')
        raise
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    if not rows:
        raise ValueError('Empty sweep calibration')
    counts = {}
    for row in rows:
        key = calibration_key(row)
        counts[key] = min(counts.get(key, int(row['operations'])), int(row['operations']))
    if output:
        Path(output).mkdir(parents=True, exist_ok=True)
        (Path(output) / 'full-calibration.stderr.log').write_text(result.stderr, encoding='utf-8')
        evidence = dict(schema=1, protocol=PROTOCOL, effective_cpus=effective_cpus or os.cpu_count() or 1,
                        strategy='Per-profile sustained3-pilot full API calibration; safety caps; minimum count for shared heap statistics keys',
                        records=rows, counts=counts)
        (Path(output) / 'full-calibration.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    return counts


def run_sweep(executable, output, trials=TRIALS, offset=0, append=False, operations_file=None,
              total_trials=None, suite=None):
    executable, output = Path(executable).resolve(), Path(output)
    profile = read_profile(executable)
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / 'sweep-environment.json'
    if append and manifest_path.exists():
        previous = json.loads(manifest_path.read_text(encoding='utf-8'))
        if previous.get('scenario_contract') != 'allocation-v3':
            raise ValueError('Cannot append current scenarios to a historical sweep inventory')
    if operations_file is None:
        counts = calibrate_sweep(executable, output)
        operations_file = output / 'sweep-operations.txt'
        operations_file.write_text(''.join(f'{key},{value}\n' for key, value in sorted(counts.items())), encoding='utf-8')
    keys = None
    for trial in range(offset + 1, offset + trials + 1):
        try:
            result = subprocess.run([str(executable), '--full', '--operations-file', str(Path(operations_file).resolve()),
                                     '--seed', str(0x20261011 + trial)], env=environment_for(profile),
                                    capture_output=True, text=True, check=True, timeout=1200)
        except Exception as error:
            (output / 'full-measurement-failures.json').write_text(json.dumps(
                failure_record(error, kind='full', trial=trial), indent=2) + '\n', encoding='utf-8')
            partial = getattr(error, 'stdout', None) or getattr(error, 'output', None) or ''
            if isinstance(partial, bytes):
                partial = partial.decode('utf-8', errors='replace')
            rows = [row for row in csv.DictReader(io.StringIO(partial)) if None not in row and all(value is not None for value in row.values())]
            if rows:
                for row in rows:
                    row['short_interval'] = str(float(row['seconds']) < .25).lower()
                write_rows(output / 'full.csv', [dict(trial=trial, **row) for row in rows], append or trial > offset + 1)
            raise
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        if not rows:
            raise ValueError('Empty sweep')
        current = set()
        for row in rows:
            row['short_interval'] = str(float(row['seconds']) < .25).lower()
            key = tuple(row[field] for field in FIELDS)
            if key in current:
                raise ValueError(f'Duplicate sweep scenario: {key}')
            current.add(key)
            elapsed = float(row['mean_ns_per_operation'])
            if not math.isfinite(elapsed) or elapsed <= 0:
                raise ValueError('Invalid sweep elapsed time')
        if keys is not None and current != keys:
            raise ValueError('Scenario coverage changed across trials')
        keys = current
        write_rows(output / 'full.csv', [dict(trial=trial, **row) for row in rows], append or trial > offset + 1)
        print(f'Sweep trial {trial}: {len(keys)} scenarios', flush=True)
    manifest = dict(schema=3, version='0.0.1', build_profile=profile, logical_cpus=os.cpu_count(),
                    source_revision=os.environ.get('GITHUB_SHA', 'unrecorded'),
                    run_id=os.environ.get('GITHUB_RUN_ID', 'unrecorded'),
                    run_url=f"https://github.com/{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/{os.environ.get('GITHUB_RUN_ID', '')}",
                    trials=total_trials or trials, scenarios_per_trial=len(keys), scenario_contract="allocation-v3",
                    affinity_pinned=suite['affinity']['pinned'] if suite else False,
                    sequential_processes=True, repetitions=1, measurement_protocol=PROTOCOL,
                    operations_file_sha256=hashlib.sha256(Path(operations_file).read_bytes()).hexdigest(),
                    measured_at_utc=datetime.now(timezone.utc).isoformat(),
                    executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    benchmark_source_sha256=hashlib.sha256((Path(__file__).resolve().parents[1] /
                        'benchmarks/allocation_bench.cpp').read_bytes().replace(b'\r\n', b'\n')).hexdigest(),
                    source_hash_line_endings='LF')
    evidence_path = output / 'full-calibration.json'
    if evidence_path.exists():
        manifest['full_calibration_sha256'] = semantic_hash(json.loads(evidence_path.read_text()))
    (output / 'sweep-environment.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--trials', type=int, default=TRIALS)
    parser.add_argument('--operations-file', type=Path)
    args = parser.parse_args()
    if args.trials < 10:
        parser.error('--trials must provide at least ten independent process samples')
    run_sweep(args.executable, args.output, args.trials, operations_file=args.operations_file)
    from benchmark_statistics import write_summary
    if (args.output / 'scaling.csv').exists():
        write_summary(args.output)


if __name__ == '__main__':
    main()
