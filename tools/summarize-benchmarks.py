"""Validate recorded measurements and emit comparison data using Python stdlib."""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path


BACKENDS = ['standard', 'mimalloc', 'jemalloc']
PHASES = ['dense', 'sparse', 'churn_dense', 'churn_sparse', 'freed']
PATHS = ['native', 'disabled', 'basic']
MIB = 1024 * 1024
SELECTED = [
    ('make_unique, 64 B', 'object', 64, 1, 1),
    ('make_shared, 64 B', 'shared_object', 64, 1, 1),
    ('vector, 32 integers', 'std_vector', 256, 8, 1),
    ('PMR vector, 32 integers', 'pmr_vector', 256, 8, 1),
    ('Zeroed Block, 4 KiB', 'zeroed', 4096, 16, 1),
    ('Block resize, 4 to 8 KiB', 'reallocate', 4096, 16, 1),
    ('Mixed lifetimes, 4-16 KiB', 'mixed_lifetime', 4096, 16, 1),
    ('Cross-thread free, 8 threads', 'cross_thread', 64, 16, 8),
]


def read_csv(path):
    with path.open(encoding='utf-8', newline='') as stream:
        return list(csv.DictReader(stream))


def median(rows, key):
    if not rows:
        raise ValueError(f'No measurements for {key}')
    return statistics.median(float(row[key]) for row in rows)


def validate_pressure(rows):
    groups = {}
    expected_requests = {}
    for row in rows:
        key = (row['trial'], row['backend'], row['path'])
        phase = row['phase']
        phases = groups.setdefault(key, set())
        if phase in phases or phase not in PHASES:
            raise ValueError(f'Duplicate or unknown phase: {key}, {phase}')
        phases.add(phase)
        live = int(row['live_blocks'])
        expected = 0 if phase == 'freed' else 1024 if 'sparse' in phase else 16384
        if live != expected or int(row['slots']) != 16384:
            raise ValueError(f'Incorrect live-block count: {key}, {phase}')
        requested = int(row['requested_bytes'])
        if requested != expected_requests.setdefault(phase, requested):
            raise ValueError('Backend/path did not receive identical requests')
        if int(row['max_requested_bytes']) < requested:
            raise ValueError('Invalid peak request count')
        if live == 0 and requested != 0:
            raise ValueError('Nonzero requests after free')
        if row['backend'] == 'standard':
            if row['usable_bytes']:
                raise ValueError('Standard usable capacity must be unknown')
        elif int(row['usable_bytes']) < requested:
            raise ValueError('Usable capacity is smaller than requested bytes')
        for field in ['rss', 'peak_rss', 'baseline_rss', 'events', 'elapsed_ns']:
            if float(row[field]) < 0:
                raise ValueError(f'Negative measurement: {field}')
    trials = sorted({key[0] for key in groups})
    expected_groups = {(trial, backend, path) for trial in trials
                       for backend in BACKENDS for path in PATHS}
    if set(groups) != expected_groups or any(set(PHASES) != phases for phases in groups.values()):
        raise ValueError('Incomplete pressure matrix')
    return len(rows)


def pressure_summary(rows):
    result = []
    for backend in BACKENDS:
        for path in PATHS:
            for phase in PHASES:
                selected = [row for row in rows if row['backend'] == backend
                            and row['path'] == path and row['phase'] == phase]
                requested = median(selected, 'requested_bytes')
                deltas = [(float(row['rss']) - float(row['baseline_rss'])) / MIB
                          for row in selected]
                usable = (median(selected, 'usable_bytes')
                          if backend != 'standard' else None)
                result.append({'backend': backend, 'path': path, 'phase': phase,
                               'requested_mib': requested / MIB,
                               'resident_delta_mib': statistics.median(deltas),
                               'resident_min_mib': min(deltas),
                               'resident_max_mib': max(deltas),
                               'usable_slack_percent':
                                   100 * (usable / requested - 1)
                                   if requested and usable is not None else None})
    return result


def summarize(root):
    output = {'platforms': {}, 'native_applications': []}
    for platform in ['windows', 'linux']:
        full = read_csv(root / platform / 'full.csv')
        expanded = root / platform
        cases = {}
        for row in full:
            key = tuple(row[field] for field in ['backend', 'workload', 'bytes',
                        'alignment', 'threads', 'statistics', 'heap'])
            trial = int(row['trial'])
            if key in cases.setdefault(trial, set()):
                raise ValueError('Duplicate full-sweep scenario')
            cases[trial].add(key)
        if set(cases) != {1, 2, 3} or any(len(keys) != 1001 for keys in cases.values()):
            raise ValueError('Published sweep requires 1001 scenarios in each of three trials')
        if cases[1] != cases[2] or cases[1] != cases[3]:
            raise ValueError('Sweep coverage changed across trials')
        latency = read_csv(expanded / 'latency.csv')
        pressure = read_csv(expanded / 'pressure.csv')
        footprint = read_csv(expanded / 'footprint.csv')
        selected = []
        for label, workload, size, alignment, threads in SELECTED:
            entry = {'label': label, 'workload': workload, 'values': {}}
            for backend in BACKENDS:
                matches = [row for row in full if row['backend'] == backend
                           and row['workload'] == workload
                           and int(row['bytes']) == size
                           and int(row['alignment']) == alignment
                           and int(row['threads']) == threads
                           and row['statistics'] == 'disabled' and row['heap'] == 'no']
                if len(matches) != 3:
                    raise ValueError(f'Ambiguous full-sweep scenario: {platform}/{backend}/{label}')
                entry['values'][backend] = median(matches, 'median_ns_per_operation')
            selected.append(entry)
        curve = []
        for size in [16, 64, 256, 4096, 65536]:
            for backend in BACKENDS:
                for path in PATHS:
                    matches = [row for row in latency if int(row['bytes']) == size
                               and row['backend'] == backend and row['path'] == path]
                    if len(matches) != 3:
                        raise ValueError('Published latency matrix needs three process trials')
                    curve.append({'backend': backend, 'path': path, 'bytes': size,
                                  'median_ns': median(matches, 'median_ns'),
                                  'trial_min_ns': min(float(row['median_ns']) for row in matches),
                                  'trial_max_ns': max(float(row['median_ns']) for row in matches)})
        memory = []
        for backend in BACKENDS:
            for heap in ['no', 'yes'] if backend != 'standard' else ['no']:
                matches = [row for row in footprint if row['backend'] == backend
                           and row['heap'] == heap and row['statistics'] == 'disabled']
                entry = {'backend': backend, 'heap': heap}
                for field in ['live_rss', 'freed_rss', 'collected_rss']:
                    entry[field] = statistics.median(
                        (int(row[field]) - int(row['baseline_rss'])) / MIB for row in matches)
                memory.append(entry)
        handoff = []
        for threads in [2, 4, 8]:
            entry = {'consumers': threads, 'values': {}}
            for backend in BACKENDS:
                matches = [row for row in full if row['backend'] == backend
                           and row['workload'] == 'cross_thread'
                           and int(row['bytes']) == 64 and int(row['alignment']) == 16
                           and int(row['threads']) == threads
                           and row['statistics'] == 'disabled' and row['heap'] == 'no']
                if len(matches) != 3:
                    raise ValueError('Incomplete handoff scenario')
                entry['values'][backend] = median(matches, 'median_ns_per_operation')
            handoff.append(entry)
        output['platforms'][platform] = {
            'latency': curve, 'workloads': selected, 'footprint': memory,
            'handoff': handoff,
            'pressure': pressure_summary(pressure),
            'validated_pressure_rows': validate_pressure(pressure), 'scenarios_per_trial': len(cases[1])}
    native = read_csv(root / 'native-applications/results.csv')
    if len(native) != 144 or any(row['exit_code'] != '0' or
            not math.isfinite(float(row['elapsed_seconds'])) or
            float(row['elapsed_seconds']) <= 0 or int(row['peak_rss_kib']) < 0
            for row in native):
        raise ValueError('Expected 144 successful, valid native application trials')
    for workload in ['cfrac', 'espresso', 'barnes', 'malloc-large', 'mstress', 'cache-thrash']:
        entry = {'workload': workload, 'values': {}}
        for backend in BACKENDS:
            matches = [row for row in native if row['workload'] == workload
                       and row['backend'] == backend]
            if len(matches) != 3 or any(row['exit_code'] != '0' for row in matches):
                raise ValueError('Incomplete or failed native application trial')
            if any(float(row['elapsed_seconds']) < 0 or int(row['peak_rss_kib']) < 0 for row in matches):
                raise ValueError('Invalid native application measurement')
            entry['values'][backend] = {'seconds': median(matches, 'elapsed_seconds'),
                                        'peak_mib': median(matches, 'peak_rss_kib') / 1024}
        output['native_applications'].append(entry)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    content = json.dumps(summarize(args.results), indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('w', encoding='utf-8', newline='\n') as file:
            file.write(content)
    else:
        print(content, end='')


if __name__ == '__main__':
    main()
