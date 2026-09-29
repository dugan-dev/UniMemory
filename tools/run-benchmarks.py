"""Run each measurement in a fresh process; no third-party Python packages."""
import argparse
import csv
import hashlib
import json
import platform
import os
import random
import subprocess
from datetime import datetime, timezone
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('executable', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--backends', nargs='+', choices=['standard', 'mimalloc', 'jemalloc'],
                    default=['standard', 'mimalloc', 'jemalloc'])
parser.add_argument('--trials', type=int, default=3)
args = parser.parse_args()
if args.trials < 1:
    parser.error('--trials must be positive')
if len(set(args.backends)) != len(args.backends):
    parser.error('--backends must not contain duplicates')
args.output.mkdir(parents=True, exist_ok=True)
executable = str(args.executable.resolve())
environment = os.environ.copy()
if platform.system() == 'Windows':
    environment['MIMALLOC_DISABLE_REDIRECT'] = '1'
details = subprocess.run([executable, 'standard', 'environment', 'disabled'],
                         env=environment, text=True, capture_output=True, check=True, timeout=30)
details = json.loads(details.stdout)
if details.get('mimalloc_redirected'):
    raise SystemExit('Explicit-backend benchmark requires mimalloc redirection disabled.')
if any(backend in details.get('standard_new_provider', '').lower()
       for backend in ['mimalloc', 'jemalloc']):
    raise SystemExit('Standard baseline was replaced by an optional allocator; '
                     'jemalloc needs --disable-cxx and mimalloc needs MI_OVERRIDE=OFF.')
manifest = {'platform': platform.platform(), 'machine': platform.machine(),
            'processor': platform.processor(), 'trials': args.trials,
            'command': Path(executable).name, 'alignment': 16, 'process_isolation': True,
            'logical_cpus': os.cpu_count(),
            'executable_sha256': hashlib.sha256(Path(executable).read_bytes()).hexdigest(),
            'allocator_environment': details,
            'windows_disable_redirect': platform.system() == 'Windows',
            'measured_at_utc': datetime.now(timezone.utc).isoformat(),
            'benchmark_source_sha256': hashlib.sha256(
                (Path(__file__).resolve().parent.parent /
                 'benchmarks/delivery_bench.cpp').read_bytes().replace(b'\r\n', b'\n')).hexdigest(),
            'source_hash_line_endings': 'LF',
            'pressure_protocol': 'mixed-size-retention-v1',
            'pressure_sizes': [17, 33, 65, 129, 257, 513, 1025, 2049, 4097, 8193],
            'pressure_slots': 16384, 'pressure_cycles': 8,
            'pressure_anchor_stride': 16, 'pressure_seed': '0xC0FFEE'}
with (args.output / 'environment.json').open('w', encoding='utf-8', newline='\n') as file:
    file.write(json.dumps(manifest, indent=2) + '\n')
cases = [(backend, path, size) for backend in args.backends
         for path in ['native', 'disabled', 'basic'] for size in [16, 64, 256, 4096, 65536]]
random.Random(0x024).shuffle(cases)
with (args.output / 'latency.csv').open('w', newline='', encoding='utf-8') as file:
    writer = None
    for trial in range(1, args.trials + 1):
        for backend, path, size in cases:
            result = subprocess.run([executable, backend, 'latency', path, str(size)],
                                    env=environment, text=True, capture_output=True, check=True, timeout=120)
            rows = list(csv.DictReader(result.stdout.splitlines()))
            if writer is None:
                writer = csv.DictWriter(file, ['trial', *rows[0].keys()], lineterminator='\n')
                writer.writeheader()
            for row in rows:
                writer.writerow({'trial': trial, **row})
with (args.output / 'footprint.csv').open('w', newline='', encoding='utf-8') as file:
    writer = None
    for trial in range(1, args.trials + 1):
        for backend in args.backends:
            for mode in ['disabled', 'basic']:
                for domain in (['plain'] if backend == 'standard' else ['plain', 'heap']):
                    result = subprocess.run([executable, backend, 'footprint', mode, domain],
                                            env=environment, text=True, capture_output=True, check=True, timeout=120)
                    rows = list(csv.DictReader(result.stdout.splitlines()))
                    if writer is None:
                        writer = csv.DictWriter(file, ['trial', *rows[0].keys()], lineterminator='\n')
                        writer.writeheader()
                    for row in rows:
                        writer.writerow({'trial': trial, **row})
with (args.output / 'heap.csv').open('w', newline='', encoding='utf-8') as file:
    writer = csv.writer(file, lineterminator='\n')
    writer.writerow(['trial', 'backend', 'operation', 'operations', 'median_ns'])
    for trial in range(1, args.trials + 1):
        for backend in args.backends:
            if backend == 'standard':
                continue
            result = subprocess.run([executable, backend, 'heap', 'disabled'],
                                    env=environment, text=True, capture_output=True, check=True, timeout=120)
            for row in csv.reader(result.stdout.splitlines()):
                writer.writerow([trial, *row])

pressure_cases = [(backend, path) for backend in args.backends
                 for path in ['native', 'disabled', 'basic']]
random.Random(0xC0FFEE).shuffle(pressure_cases)
with (args.output / 'pressure.csv').open('w', newline='', encoding='utf-8') as file:
    writer = None
    for trial in range(1, args.trials + 1):
        for backend, path in pressure_cases:
            result = subprocess.run([executable, backend, 'pressure', path],
                                    env=environment, text=True, capture_output=True,
                                    check=True, timeout=120)
            rows = list(csv.DictReader(result.stdout.splitlines()))
            if writer is None:
                writer = csv.DictWriter(file, ['trial', *rows[0].keys()], lineterminator='\n')
                writer.writeheader()
            for row in rows:
                writer.writerow({'trial': trial, **row})
