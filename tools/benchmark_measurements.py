"""Fresh-process Delivery measurements shared by local and controlled CI runners."""
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
from datetime import datetime, timezone
from benchmark_profile import read_profile, environment_for
from benchmark_host import cpu_model

TRIALS = 10
SIZES = (16, 64, 256, 4096, 65536)
THREADS = (1, 2, 4, 8, 16)
TARGET_SECONDS = .35
PROTOCOL = 'native-api-calibrated-v3'
ROOT = Path(__file__).resolve().parents[1]


def percentile(values, fraction):
    if not values or not 0 < fraction <= 1:
        raise ValueError('Invalid percentile input')
    return sorted(values)[math.ceil(len(values) * fraction) - 1]


def measure(executable, backend, workload, path, *parameters):
    profile = read_profile(executable)
    result = subprocess.run([str(executable), backend, workload, path, *map(str, parameters)],
                            env=environment_for(profile), capture_output=True, text=True,
                            check=True, timeout=600)
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    if not rows:
        raise RuntimeError('Benchmark produced no records')
    return rows


def write_rows(path, rows, append=False, fields=None):
    path = Path(path)
    if not rows and fields is None:
        raise ValueError('Empty measurements')
    existing = append and path.exists()
    fields = fields or list(rows[0])
    if existing:
        with path.open(newline='', encoding='utf-8') as file:
            if next(csv.reader(file)) != fields:
                raise ValueError('Measurement schema changed while appending')
    with path.open('a' if existing else 'w', newline='', encoding='utf-8') as file:
        writer = csv.DictWriter(file, fields, lineterminator='\n')
        if not existing:
            writer.writeheader()
        writer.writerows(rows)


def calibrate_delivery(executable):
    profile = read_profile(executable)
    backend, paths = profile['backend'], ('native', profile['api_path'])
    result = {'latency': {}, 'scaling': {}, 'heap': {}}
    for size in SIZES:
        for path in paths:
            row = measure(executable, backend, 'latency', path, size, 20000)[0]
            count = max(20000, math.ceil(TARGET_SECONDS * 1e9 / float(row['mean_ns'])))
            result['latency'][str(size)] = max(result['latency'].get(str(size), 0), count)
    for workload in ('scaling', 'handoff'):
        for size in (64, 4096, 65536):
            for threads in THREADS:
                key = f'{workload}:{size}:{threads}'
                for path in paths:
                    row = measure(executable, backend, workload, path, size, threads, 8)[0]
                    rounds = max(8, math.ceil(8 * TARGET_SECONDS / float(row['seconds'])))
                    result['scaling'][key] = max(result['scaling'].get(key, 0), rounds)
    if backend != 'standard':
        for operation in ('owns', 'collect', 'reset'):
            row = measure(executable, backend, 'heap', 'disabled', operation, 2000)[0]
            result['heap'][operation] = max(2000, math.ceil(TARGET_SECONDS * 1e9 / float(row['mean_ns'])))
    return result


def record_environment(executable, output, total_trials, calibration, suite=None):
    profile = read_profile(executable)
    details = measure_environment(executable, profile)
    compiler_files = list(executable.parent.parent.glob('CMakeFiles/*/CMakeCXXCompiler.cmake'))
    if not compiler_files:
        compiler_files = list(executable.parent.glob('CMakeFiles/*/CMakeCXXCompiler.cmake'))
    if len(compiler_files) != 1:
        raise RuntimeError('Missing benchmark compiler provenance')
    compiler = dict(re.findall(r'set\((CMAKE_CXX_(?:COMPILER_ID|COMPILER_VERSION)) "([^"\n]+)"\)',
                               compiler_files[0].read_text()))
    source_hash = hashlib.sha256((ROOT / 'benchmarks/delivery_bench.cpp').read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    manifest = dict(schema=3, platform=platform.platform(), machine=platform.machine(),
                    processor=platform.processor(), architecture=platform.machine(), cpu_model=cpu_model(),
                    compiler=compiler, trials=total_trials, command=executable.name, alignment=16,
                    process_isolation=True, logical_cpus=os.cpu_count(),
                    executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    allocator_environment=details, build_profile=profile,
                    windows_disable_redirect=platform.system() == 'Windows',
                    measured_at_utc=datetime.now(timezone.utc).isoformat(),
                    benchmark_source_sha256=source_hash, benchmark_sha256=source_hash, source_hash_line_endings='LF',
                    source_revision=os.environ.get('GITHUB_SHA', 'unrecorded'),
                    run_id=os.environ.get('GITHUB_RUN_ID', 'unrecorded'),
                    run_url=f"https://github.com/{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/{os.environ.get('GITHUB_RUN_ID', '')}",
                    measurement_protocol=PROTOCOL, calibration=calibration, target_interval_seconds=.25,
                    calibration_target_seconds=TARGET_SECONDS,
                    pressure_protocol='mixed-size-retention-v1',
                    pressure_sizes=[17, 33, 65, 129, 257, 513, 1025, 2049, 4097, 8193],
                    pressure_slots=16384, pressure_cycles=8, pressure_anchor_stride=16, pressure_seed='0xC0FFEE',
                    threads=list(THREADS), sizes=list(SIZES), backends=[profile['backend']],
                    paths=['native', profile['api_path']], tail_samples=8192,
                    tail_method='8192 individual allocate/free intervals; clock overhead retained separately',
                    trace_storage=dict(location='workflow artifacts', retention_days=90),
                    scaling_method='Calibrated common rounds; synchronized batches of512;512MiB maximum live payload; thread creation,warmup,join excluded',
                    performance_gate='Report only; all trials retained, short intervals disclosed',
                    statistic='Arithmetic mean of independent process intervals; no sample removal')
    if suite:
        manifest['controlled_suite'] = suite
        (output / 'controlled-suite.json').write_text(json.dumps(suite, indent=2) + '\n', encoding='utf-8')
    (output / 'environment.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return profile


def measure_environment(executable, profile):
    result = subprocess.run([str(executable), profile['backend'], 'environment', profile['api_path']],
                            env=environment_for(profile), text=True, capture_output=True, check=True, timeout=30)
    details = json.loads(result.stdout)
    if details.get('mimalloc_redirected') or any(backend in details.get('standard_new_provider', '').lower()
                                             for backend in ('mimalloc', 'jemalloc')):
        raise ValueError('Explicit benchmark SDKs must not replace standard malloc/new')
    return details


def finalize_traces(output):
    output = Path(output)
    manifest_path = output / 'environment.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    evidence = {}
    for trial in range(1, manifest['trials'] + 1):
        for path in manifest['paths']:
            for size in SIZES:
                name = f"raw/{trial}-{manifest['build_profile']['backend']}-{path}-{size}.csv.gz"
                evidence[name] = dict(sha256=hashlib.sha256((output / name).read_bytes()).hexdigest(), samples=8192)
    manifest['tail_trace_evidence'] = evidence
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


def run_base(executable, output, trials=TRIALS, offset=0, append=False, calibration=None, total_trials=None, suite=None):
    executable, output = Path(executable).resolve(), Path(output)
    output.mkdir(parents=True, exist_ok=True)
    calibration = calibration or calibrate_delivery(executable)
    profile = read_profile(executable)
    if not append or not (output / 'environment.json').exists():
        record_environment(executable, output, total_trials or trials, calibration, suite)
    backend, paths = profile['backend'], ('native', profile['api_path'])
    for trial in range(offset + 1, offset + trials + 1):
        rng = random.Random(0x20261011 + trial)
        sizes = list(SIZES)
        rng.shuffle(sizes)
        ordered_paths = paths if trial % 2 else tuple(reversed(paths))
        cases = [(path, size) for size in sizes for path in ordered_paths]
        latency = []
        for path, size in cases:
            latency.extend(dict(trial=trial, **row) for row in measure(executable, backend, 'latency', path,
                                                                      size, calibration['latency'][str(size)]))
        write_rows(output / 'latency.csv', latency, append or trial > offset + 1)
        footprints = []
        modes = [profile['api_path']] if backend == 'standard' else (
            ['disabled', 'basic'] if profile['statistics'] == 'ON' else ['disabled'])
        cases = [(mode, domain) for mode in modes for domain in (
            ['plain'] if backend == 'standard' else ['plain', 'heap'] if mode == profile['api_path'] else ['heap'])]
        rng.shuffle(cases)
        for mode, domain in cases:
            footprints.extend(dict(trial=trial, **row) for row in measure(executable, backend, 'footprint', mode, domain))
        write_rows(output / 'footprint.csv', footprints, append or trial > offset + 1)
        heaps = []
        operations = ['owns', 'collect', 'reset']
        rng.shuffle(operations)
        if backend != 'standard':
            for operation in operations:
                heaps.extend(dict(trial=trial, **row) for row in measure(executable, backend, 'heap', 'disabled',
                                                                       operation, calibration['heap'][operation]))
        write_rows(output / 'heap.csv', heaps, append or trial > offset + 1,
                   ['trial', 'backend', 'operation', 'operations', 'median_ns', 'min_ns', 'mean_ns', 'max_ns', 'samples_ns', 'seconds'])
        pressure = []
        ordered_paths = list(paths)
        rng.shuffle(ordered_paths)
        for path in ordered_paths:
            pressure.extend(dict(trial=trial, **row) for row in measure(executable, backend, 'pressure', path))
        write_rows(output / 'pressure.csv', pressure, append or trial > offset + 1)


def run_scaling_tails(executable, output, trials=TRIALS, offset=0, append=False, calibration=None):
    executable, output = Path(executable).resolve(), Path(output)
    calibration = calibration or calibrate_delivery(executable)
    profile = read_profile(executable)
    backend, paths = profile['backend'], ('native', profile['api_path'])
    raw = output / 'raw'
    raw.mkdir(exist_ok=True)
    for trial in range(offset + 1, offset + trials + 1):
        rng = random.Random(0x20261011 + trial)
        groups = [(workload, size, threads) for workload in ('scaling', 'handoff')
                  for size in (64, 4096, 65536) for threads in THREADS]
        rng.shuffle(groups)
        ordered_paths = paths if trial % 2 else tuple(reversed(paths))
        cases = [(path, workload, size, threads) for workload, size, threads in groups for path in ordered_paths]
        scaling = []
        for path, workload, size, threads in cases:
            rounds = calibration['scaling'][f'{workload}:{size}:{threads}']
            rows = measure(executable, backend, workload, path, size, threads, rounds)
            if len(rows) != 1 or int(rows[0]['operations']) != threads * 512 * rounds:
                raise ValueError('Scaling changed its calibrated operation count')
            scaling.append(dict(trial=trial, **rows[0]))
        write_rows(output / 'scaling.csv', scaling, append or trial > offset + 1)
        sizes = list(SIZES)
        rng.shuffle(sizes)
        cases = [(path, size) for size in sizes for path in ordered_paths]
        tails = []
        for path, size in cases:
            rows = measure(executable, backend, 'tails', path, size)
            if len(rows) != 8192:
                raise ValueError('Incomplete individual-operation samples')
            buffer = io.StringIO()
            writer = csv.DictWriter(buffer, list(rows[0]), lineterminator='\n')
            writer.writeheader()
            writer.writerows(rows)
            (raw / f'{trial}-{backend}-{path}-{size}.csv.gz').write_bytes(gzip.compress(buffer.getvalue().encode(), mtime=0))
            allocation = [float(row['allocate_ns']) for row in rows]
            release = [float(row['free_ns']) for row in rows]
            tails.append(dict(trial=trial, backend=backend, path=path, bytes=size, samples=len(rows),
                              allocate_p50_ns=percentile(allocation, .5), allocate_p95_ns=percentile(allocation, .95),
                              allocate_p99_ns=percentile(allocation, .99), allocate_p999_ns=percentile(allocation, .999),
                              allocate_max_ns=max(allocation), free_p99_ns=percentile(release, .99),
                              clock_p50_ns=statistics.median(float(row['clock_ns']) for row in rows)))
        write_rows(output / 'tails.csv', tails, append or trial > offset + 1)
        print(f'Delivery performance trial {trial}: complete', flush=True)
