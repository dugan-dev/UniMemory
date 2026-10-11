"""Measure all six SDK/statistics profiles sequentially on the same physical runner."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys
from benchmark_host import control_host
from benchmark_measurements import (calibrate_delivery, run_base, run_scaling_tails,
                                    finalize_traces, PROTOCOL, TRIALS)
from benchmark_profile import read_profile, environment_for
from benchmark_statistics import write_summary

spec = importlib.util.spec_from_file_location('sweep_driver', Path(__file__).with_name('run-sweep.py'))
sweep_driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sweep_driver)
report_spec = importlib.util.spec_from_file_location('measurement_report', Path(__file__).parent / 'ci/performance-report.py')
measurement_report = importlib.util.module_from_spec(report_spec)
report_spec.loader.exec_module(measurement_report)


def schedule(profiles, trials, seed):
    rows = []
    generator = random.Random(seed)
    for trial in range(1, trials + 1):
        order = list(profiles)
        generator.shuffle(order)
        rows.extend(dict(trial=trial, backend=backend, statistics=mode) for backend, mode in order)
    return rows


def merge_calibration(records):
    result = {}
    for record in records:
        for key, value in record.items():
            if int(value) <= 0:
                raise ValueError('Calibration requires positive equal operation counts')
            result[key] = max(result.get(key, 0), int(value))
    return result


def executables(build_root):
    result = {}
    evidence = []
    common = None
    sdk_hashes = {}
    for backend in ('standard', 'mimalloc', 'jemalloc'):
        for mode in ('OFF', 'ON'):
            directory = build_root / f'{backend}-{mode}'
            delivery, sweep = directory / 'UniMemoryDeliveryBenchmark', directory / 'UniMemoryBenchmark'
            if not delivery.is_file():
                delivery, sweep = directory / 'Release/UniMemoryDeliveryBenchmark.exe', directory / 'Release/UniMemoryBenchmark.exe'
            if not delivery.is_file() or not sweep.is_file():
                raise ValueError(f'Missing paired benchmark executables: {directory}')
            profile = read_profile(delivery)
            if (profile['backend'], profile['statistics']) != (backend, mode):
                raise ValueError('Executable disagrees with controlled profile directory')
            if profile['configuration'] != 'Release' or profile['checks'] != 'AUTO':
                raise ValueError('Controlled benchmark requires matching Release/AUTO builds')
            record = profile['build_parameters']
            contract = tuple(record.get(key) for key in (
                'compiler', 'msvc_runtime', 'flags', 'release_flags', 'ipo', 'compile_options'))
            if common is not None and contract != common:
                raise ValueError('Compiler, CRT, optimization or target options differ across SDK builds')
            common = contract
            binaries = {key: value['sha256'] for key, value in profile['sdk_binaries'].items()}
            if backend in sdk_hashes and sdk_hashes[backend] != binaries:
                raise ValueError('Statistics modes must use the exact same SDK binary')
            sdk_hashes[backend] = binaries
            result[backend, mode] = delivery.resolve(), sweep.resolve()
            evidence.append(dict(backend=backend, statistics=mode, configuration=profile['configuration'],
                                 compiler=record.get('compiler'), msvc_runtime=record.get('msvc_runtime'),
                                 release_flags=record.get('release_flags'), sdk_binaries=binaries,
                                 flags=record.get('flags'), ipo=record.get('ipo'), compile_options=record.get('compile_options'),
                                 executable_sha256=hashlib.sha256(delivery.read_bytes()).hexdigest(),
                                 sweep_executable_sha256=hashlib.sha256(sweep.read_bytes()).hexdigest(),
                                 sweep_source_sha256=hashlib.sha256((Path(__file__).resolve().parents[1] /
                                     'benchmarks/allocation_bench.cpp').read_bytes().replace(b'\r\n', b'\n')).hexdigest()))
    return result, evidence


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('build_root', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--trials', type=int, default=TRIALS)
    parser.add_argument('--seed', type=int, default=0x20261011)
    args = parser.parse_args()
    if args.trials < 10:
        parser.error('Controlled SDK comparisons require at least ten independent process trials')
    args.output.mkdir(parents=True, exist_ok=True)
    pairs, evidence = executables(args.build_root.resolve())
    host = control_host()
    delivery_calibrations, full_files = {}, {}
    for key, (delivery, sweep) in pairs.items():
        print(f'Calibrating common operation counts: {key}', flush=True)
        delivery_calibrations[key] = calibrate_delivery(delivery)
        counts = sweep_driver.calibrate_sweep(sweep)
        directory = args.output / 'profiles' / f'{key[0]}-{key[1]}'
        directory.mkdir(parents=True, exist_ok=True)
        full_file = directory / 'sweep-operations.txt'
        full_file.write_text(''.join(f'{case},{count}\n' for case, count in sorted(counts.items())), encoding='utf-8')
        full_files[key] = full_file
        next(item for item in evidence if (item['backend'], item['statistics']) == key)['full_operations_sha256'] = hashlib.sha256(full_file.read_bytes()).hexdigest()
        next(item for item in evidence if (item['backend'], item['statistics']) == key)['heap_operation_counts'] = delivery_calibrations[key]['heap']
    calibration = {table: merge_calibration(record[table] for record in delivery_calibrations.values())
                   for table in ('latency', 'scaling')}
    suite = dict(schema=3, protocol=PROTOCOL, trials=args.trials, seed=args.seed, profiles=evidence,
                 schedule=schedule(pairs, args.trials, args.seed), calibration=calibration,
                 full_calibration='Per SDK/statistics profile; calibrated interval normalized per complete operation',
                 calibrated_on_all_profiles=True, target_interval_seconds=.25,
                 short_intervals='Retained and disclosed; calibration cannot remove hosted runner noise',
                 **host)
    (args.output / 'controlled-suite.json').write_text(json.dumps(suite, indent=2) + '\n', encoding='utf-8')
    for item in suite['schedule']:
        trial, backend, mode = item['trial'], item['backend'], item['statistics']
        delivery, sweep = pairs[backend, mode]
        output = args.output / 'profiles' / f'{backend}-{mode}'
        profile_calibration = dict(calibration, heap=delivery_calibrations[backend, mode]['heap'])
        print(f"Controlled trial{trial}/{args.trials}: {backend}/{mode}", flush=True)
        run_base(delivery, output, 1, trial - 1, trial > 1, profile_calibration, args.trials, suite)
        run_scaling_tails(delivery, output, 1, trial - 1, trial > 1, calibration)
        sweep_driver.run_sweep(sweep, output, 1, trial - 1, trial > 1, full_files[backend, mode], args.trials, suite)
    for key, (delivery, _) in pairs.items():
        output = args.output / 'profiles' / f'{key[0]}-{key[1]}'
        finalize_traces(output)
        write_summary(output)
        # Capture code generation from the same executable, once after all timing.
        subprocess.run([sys.executable, str(Path(__file__).parent / 'ci/codegen-evidence.py'),
                        str(delivery), str(output)], env=environment_for(read_profile(delivery)), check=True)
        measurement_report.validate(output, strict_artifact=True)


if __name__ == '__main__':
    main()
