"""Measure each unified API scenario in fresh processes, using Python stdlib."""
import argparse
import csv
import hashlib
import json
import math
import os
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('executable', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--trials', type=int, default=3)
args = parser.parse_args()
if args.trials < 1:
    parser.error('--trials must be positive')
args.output.mkdir(parents=True, exist_ok=True)
executable = args.executable.resolve()
environment = os.environ.copy()
if platform.system() == 'Windows':
    environment['MIMALLOC_DISABLE_REDIRECT'] = '1'
keys = None
with (args.output / 'full.csv').open('w', newline='', encoding='utf-8') as output:
    writer = None
    for trial in range(1, args.trials + 1):
        current = set()
        modes = [[], ['--basic']] if trial % 2 else [['--basic'], []]
        for mode in modes:
            result = subprocess.run([str(executable), '--full', *mode],
                                    env=environment, capture_output=True, text=True,
                                    check=True, timeout=600)
            rows = list(csv.DictReader(result.stdout.splitlines()))
            if not rows:
                raise ValueError('Empty sweep')
            if writer is None:
                writer = csv.DictWriter(output, ['trial', *rows[0].keys()], lineterminator='\n')
                writer.writeheader()
            for row in rows:
                key = tuple(row[field] for field in ['backend', 'workload', 'bytes',
                            'alignment', 'threads', 'statistics', 'heap'])
                if key in current:
                    raise ValueError(f'Duplicate scenario: {key}')
                current.add(key)
                elapsed = float(row['median_ns_per_operation'])
                if not math.isfinite(elapsed) or elapsed < 0:
                    raise ValueError('Invalid elapsed time')
                writer.writerow({'trial': trial, **row})
        if keys is not None and keys != current:
            raise ValueError('Scenario coverage changed across trials')
        keys = current
        output.flush()
        print(f'Sweep trial {trial}: {len(keys)} scenarios', flush=True)

manifest = {
    'version': '0.0.1', 'platform': platform.platform(),
    'logical_cpus': os.cpu_count(), 'trials': args.trials,
    'scenarios_per_trial': len(keys), 'affinity_pinned': False,
    'sequential_processes': True, 'repetitions': 7,
    'measured_at_utc': datetime.now(timezone.utc).isoformat(),
    'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
    'benchmark_source_sha256': hashlib.sha256(
        (Path(__file__).resolve().parents[1] / 'benchmarks/allocation_bench.cpp').read_bytes().replace(b'\r\n', b'\n')).hexdigest(),
    'source_hash_line_endings': 'LF',
}
with (args.output / 'sweep-environment.json').open('w', encoding='utf-8', newline='\n') as file:
    file.write(json.dumps(manifest, indent=2) + '\n')
