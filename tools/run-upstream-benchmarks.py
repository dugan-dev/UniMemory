"""Run packaged upstream programs using Linux allocator interposition.

This measures native allocators through malloc/new, not the UniMemory interface.
All commands, exit codes and logs are retained; no external Python packages.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('binaries', type=Path)
parser.add_argument('sources', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--mimalloc', type=Path, required=True)
parser.add_argument('--jemalloc', type=Path, required=True)
parser.add_argument('--trials', type=int, default=3)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
threads = 4
cases = {
    'cfrac': ['17545186520507317056371138836327483792789528'],
    'espresso': [str(args.sources.resolve() / 'espresso/largest.espresso')],
    'barnes': [],
    'larson': ['5', '8', '1000', '5000', '100', '4141', str(threads)],
    'larson-sized': ['5', '8', '1000', '5000', '100', '4141', str(threads)],
    'alloc-test': [str(threads)],
    'cache-scratch': [str(threads), '1000', '1', '2000000', str(threads)],
    'cache-thrash': [str(threads), '1000', '1', '2000000', str(threads)],
    'xmalloc-test': ['-w', str(threads), '-t', '5', '-s', '64'],
    'malloc-large-old': [],
    'malloc-large': [],
    'mstress': [str(threads), '50', '25'],
    'mleak': ['1'],
    'rptest': [str(threads), '0', '1', '2', '500', '1000', '100', '8', '16000'],
    'glibc-simple': [],
    'glibc-thread': [str(threads)],
}
libraries = {'standard': None, 'mimalloc': str(args.mimalloc.resolve()),
             'jemalloc': str(args.jemalloc.resolve())}
manifest = {
    'source': 'https://github.com/daanx/mimalloc-bench',
    'revision': 'ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86',
    'platform': platform.platform(), 'threads': threads, 'trials': args.trials,
    'commands': {name: (['espresso/largest.espresso'] if name == 'espresso' else command)
                 for name, command in cases.items()},
    'interface': 'native malloc/new through LD_PRELOAD',
    'elapsed_clock': 'Python perf_counter_ns (monotonic, includes process startup)',
    'excluded': {
        'sh6bench/sh8bench': 'Licensed sources not distributed in the collection.',
        'security': 'Intentional undefined behavior; not valid allocation workloads.',
        'externally downloaded applications': 'Require additional dependencies outside this suite.'},
    'libraries': {name: Path(library).name if library else None
                  for name, library in libraries.items()},
}
with (args.output / 'environment.json').open('w', encoding='utf-8', newline='\n') as file:
    file.write(json.dumps(manifest, indent=2) + '\n')
schedule = [(backend, case) for backend in libraries for case in cases]
random.Random(0x024).shuffle(schedule)
failed = False
with (args.output / 'results.csv').open('w', newline='', encoding='utf-8') as file:
    writer = csv.writer(file, lineterminator='\n')
    writer.writerow(['trial', 'backend', 'workload', 'elapsed_seconds', 'user_seconds',
                     'system_seconds', 'peak_rss_kib', 'exit_code'])
    for trial in range(1, args.trials + 1):
        for backend, case in schedule:
            prefix = (args.output / f'{trial}-{backend}-{case}').resolve()
            working_directory = Path(str(prefix) + '.files')
            working_directory.mkdir(exist_ok=True)
            environment = os.environ.copy()
            environment.pop('LD_PRELOAD', None)
            library = libraries[backend]
            if library:
                environment['LD_PRELOAD'] = library
            command = ['/usr/bin/time', '-f', '%e,%U,%S,%M,%x', '-o', str(prefix) + '.time',
                       str(args.binaries.resolve() / case), *cases[case]]
            with open(str(prefix) + '.log', 'w') as log:
                stdin = open(args.sources / 'barnes/input', 'rb') if case == 'barnes' else None
                try:
                    started = time.perf_counter_ns()
                    result = subprocess.run(command, env=environment, stdin=stdin, stdout=log,
                                            cwd=working_directory,
                                            stderr=subprocess.STDOUT, timeout=600)
                    elapsed = (time.perf_counter_ns() - started) / 1e9
                    timing = Path(str(prefix) + '.time').read_text().splitlines()[-1].split(',')
                    writer.writerow([trial, backend, case, elapsed, *timing[1:]])
                    failed |= result.returncode != 0
                except subprocess.TimeoutExpired:
                    writer.writerow([trial, backend, case, '', '', '', '', 'timeout'])
                    failed = True
                finally:
                    if stdin:
                        stdin.close()
            file.flush()
            print(f'{trial} {backend} {case}', flush=True)
if failed:
    raise SystemExit('At least one upstream workload failed; inspect results and logs.')
