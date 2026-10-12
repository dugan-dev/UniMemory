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
from benchmark_calibration import (PROTOCOL, TARGET_SECONDS, PILOT_TIMEOUT_SECONDS,
                                   NORMAL_ROUND_LIMIT, OVERSUBSCRIBED_ROUND_LIMIT,
                                   OPERATION_LIMIT, calibrate_pair, new_evidence, failure_record)

TRIALS = 10
SIZES = (16, 64, 256, 4096, 65536)
THREADS = (1, 2, 4, 8, 16)
ROOT = Path(__file__).resolve().parents[1]


def percentile(values, fraction):
    if not values or not 0 < fraction <= 1:
        raise ValueError('Invalid percentile input')
    return sorted(values)[math.ceil(len(values) * fraction) - 1]


def measure(executable, backend, workload, path, *parameters, timeout=600):
    profile = read_profile(executable)
    result = subprocess.run([str(executable), backend, workload, path, *map(str, parameters)],
                            env=environment_for(profile), capture_output=True, text=True,
                            check=True, timeout=timeout)
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


def calibration_path(output):
    return Path(output) / 'calibration-pilots.json'


def scaling_canary(executable, output, effective_cpus):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    profile = read_profile(executable)
    evidence = new_evidence(effective_cpus)
    evidence['canary_records'] = []
    path = calibration_path(output)
    for api_path in ('native', profile['api_path']):
        identity = dict(kind='canary', case='scaling:64:8', path=api_path, count=32, repeat=1)
        try:
            row = measure(executable, profile['backend'], 'scaling', api_path, 64, 8, 32,
                          timeout=PILOT_TIMEOUT_SECONDS)[0]
            evidence['canary_records'].append(dict(identity, row=row))
        except Exception as error:
            evidence['failures'].append(failure_record(error, **identity))
            path.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
            raise
    path.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')


def calibrate_delivery(executable, output=None, effective_cpus=None):
    profile = read_profile(executable)
    backend, paths = profile['backend'], ('native', profile['api_path'])
    result = {'latency': {}, 'scaling': {}, 'heap': {}}
    effective_cpus = effective_cpus or os.cpu_count() or 1
    path = calibration_path(output) if output else None
    evidence = json.loads(path.read_text()) if path and path.exists() else new_evidence(effective_cpus)
    if path:
        path.parent.mkdir(parents=True, exist_ok=True)
    def save():
        if path:
            path.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    for size in SIZES:
        decision = calibrate_pair('latency', str(size), paths,
            lambda api_path, count, repeat: measure(executable, backend, 'latency', api_path, size, count,
                                                   timeout=PILOT_TIMEOUT_SECONDS)[0],
            initial=20000, count_limit=OPERATION_LIMIT, evidence=evidence, save=save)
        result['latency'][str(size)] = decision['count']
    for workload in ('scaling', 'handoff'):
        for size in (64, 4096, 65536):
            for threads in THREADS:
                key = f'{workload}:{size}:{threads}'
                oversubscribed = threads > effective_cpus
                limit = OVERSUBSCRIBED_ROUND_LIMIT if oversubscribed else NORMAL_ROUND_LIMIT
                decision = calibrate_pair('scaling', key, paths,
                    lambda api_path, count, repeat: measure(executable, backend, workload, api_path,
                                                           size, threads, count, timeout=PILOT_TIMEOUT_SECONDS)[0],
                    initial=8, count_limit=limit, oversubscribed=oversubscribed, evidence=evidence, save=save)
                result['scaling'][key] = decision['count']
    for operation in ('owns', 'collect', 'reset'):
        decision = calibrate_pair('heap', operation, ('disabled',),
            lambda api_path, count, repeat: measure(executable, backend, 'heap', api_path,
                                       operation, count, timeout=PILOT_TIMEOUT_SECONDS)[0],
            initial=2000, count_limit=OPERATION_LIMIT, evidence=evidence, save=save)
        result['heap'][operation] = decision['count']
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
                    pressure_protocol='mixed-size-retention-v1', scenario_contract='delivery-v2',
                    pressure_sizes=[17, 33, 65, 129, 257, 513, 1025, 2049, 4097, 8193],
                    pressure_slots=16384, pressure_cycles=8, pressure_anchor_stride=16, pressure_seed='0xC0FFEE',
                    threads=list(THREADS), sizes=list(SIZES), backends=[profile['backend']],
                    paths=['native', profile['api_path']], tail_samples=8192,
                    tail_method='8192 individual allocate/free intervals; clock overhead retained separately',
                    trace_storage=dict(location='workflow artifacts', retention_days=90),
                    scaling_method='Per-profile paired rounds; sustained3-pilot calibration; disclosed safety caps; synchronized512-block batches;512MiB maximum live payload; thread creation,warmup,join excluded',
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


def measured_rows(executable, output, trial, backend, workload, api_path, *parameters):
    try:
        rows = measure(executable, backend, workload, api_path, *parameters, timeout=30 if workload in ('scaling', 'handoff') else 600)
    except Exception as error:
        path = Path(output) / 'measurement-failures.json'
        failures = json.loads(path.read_text()) if path.exists() else []
        failures.append(failure_record(error, trial=trial, backend=backend, workload=workload,
                                       path=api_path, parameters=list(parameters)))
        path.write_text(json.dumps(failures, indent=2) + '\n', encoding='utf-8')
        raise
    for row in rows:
        if 'seconds' in row:
            row['short_interval'] = str(float(row['seconds']) < .25).lower()
    return rows


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
    if append and (output / 'environment.json').exists():
        previous = json.loads((output / 'environment.json').read_text(encoding='utf-8'))
        if previous.get('scenario_contract') != 'delivery-v2':
            raise ValueError('Cannot append current scenarios to a historical delivery inventory')
    calibration = calibration or calibrate_delivery(executable, output)
    profile = read_profile(executable)
    if not append or not (output / 'environment.json').exists():
        record_environment(executable, output, total_trials or trials, calibration, suite)
    backend, paths = profile['backend'], ('native', profile['api_path'])
    for trial in range(offset + 1, offset + trials + 1):
        written = set()
        def persist(table, rows, fields=None):
            write_rows(output / f'{table}.csv', [dict(trial=trial, **row) for row in rows],
                       append or trial > offset + 1 or table in written, fields)
            written.add(table)
        rng = random.Random(0x20261011 + trial)
        sizes = list(SIZES)
        rng.shuffle(sizes)
        ordered_paths = paths if trial % 2 else tuple(reversed(paths))
        cases = [(path, size) for size in sizes for path in ordered_paths]
        for path, size in cases:
            persist('latency', measured_rows(executable, output, trial, backend, 'latency', path,
                                            size, calibration['latency'][str(size)]))
        modes = ['disabled', 'basic'] if profile['statistics'] == 'ON' else ['disabled']
        cases = [(mode, domain) for mode in modes for domain in (
            ['plain', 'heap'] if mode == profile['api_path'] else ['heap'])]
        rng.shuffle(cases)
        for mode, domain in cases:
            persist('footprint', measured_rows(executable, output, trial, backend, 'footprint', mode, domain))
        operations = ['owns', 'collect', 'reset']
        rng.shuffle(operations)
        for operation in operations:
            persist('heap', measured_rows(executable, output, trial, backend, 'heap', 'disabled',
                                         operation, calibration['heap'][operation]))
        ordered_paths = list(paths)
        rng.shuffle(ordered_paths)
        for path in ordered_paths:
            persist('pressure', measured_rows(executable, output, trial, backend, 'pressure', path))


def run_scaling_tails(executable, output, trials=TRIALS, offset=0, append=False, calibration=None):
    executable, output = Path(executable).resolve(), Path(output)
    calibration = calibration or calibrate_delivery(executable, output)
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
        for case_index, (path, workload, size, threads) in enumerate(cases):
            rounds = calibration['scaling'][f'{workload}:{size}:{threads}']
            rows = measured_rows(executable, output, trial, backend, workload, path, size, threads, rounds)
            write_rows(output / 'scaling.csv', [dict(trial=trial, **row) for row in rows],
                       append or trial > offset + 1 or case_index > 0)
            if len(rows) != 1 or int(rows[0]['operations']) != threads * 512 * rounds:
                raise ValueError('Scaling changed its calibrated operation count')
        sizes = list(SIZES)
        rng.shuffle(sizes)
        cases = [(path, size) for size in sizes for path in ordered_paths]
        tails = []
        for path, size in cases:
            rows = measured_rows(executable, output, trial, backend, 'tails', path, size)
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
