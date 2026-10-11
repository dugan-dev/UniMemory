"""Statistics over independent process trials; never trim or winsorize samples."""
from collections import defaultdict
import csv
import json
import math
from pathlib import Path
import random
import statistics
from result_io import read_csv

SCENARIO_FIELDS = {
    'latency': ('backend', 'path', 'bytes'),
    'scaling': ('backend', 'path', 'workload', 'bytes', 'threads'),
    'tails': ('backend', 'path', 'bytes'),
    'pressure': ('backend', 'path', 'phase'),
    'footprint': ('backend', 'statistics', 'heap'),
    'heap': ('backend', 'operation'),
    'full': ('backend', 'workload', 'bytes', 'alignment', 'threads', 'statistics', 'heap'),
    'native': ('backend', 'workload'),
}
METRICS = {
    'latency': ('mean_ns',),
    'scaling': ('operations_per_second', 'seconds', 'baseline_rss', 'final_rss', 'peak_rss', 'delta_final_rss'),
    'tails': ('allocate_p50_ns', 'allocate_p95_ns', 'allocate_p99_ns', 'allocate_p999_ns',
              'allocate_max_ns', 'free_p99_ns', 'clock_p50_ns'),
    'pressure': ('baseline_rss', 'rss', 'peak_rss', 'delta_rss', 'elapsed_ns'),
    'footprint': ('baseline_rss', 'live_rss', 'freed_rss', 'collected_rss', 'peak_rss',
                  'delta_live_rss', 'delta_freed_rss', 'delta_collected_rss'),
    'heap': ('mean_ns',),
    'full': ('mean_ns_per_operation',),
    'native': ('elapsed_seconds', 'user_seconds', 'system_seconds', 'peak_rss_kib'),
}
BOOTSTRAP_RESAMPLES = 10000
BOOTSTRAP_SEED = 0x20261011


def numeric_summary(values, bootstrap=True):
    values = list(map(float, values))
    if len(values) < 2 or any(not math.isfinite(value) for value in values):
        raise ValueError('Statistics require at least two finite independent samples')
    mean = statistics.fmean(values)
    sd = statistics.stdev(values)
    result = dict(n=len(values), min=min(values), max=max(values), mean=mean,
                  median=statistics.median(values), sample_sd=sd,
                  cv=sd / abs(mean) if mean else None)
    if bootstrap:
        if result['min'] == result['max']:
            low = high = mean
        else:
            generator = random.Random(BOOTSTRAP_SEED)
            count = len(values)
            means = sorted(statistics.fmean(generator.choices(values, k=count))
                           for _ in range(BOOTSTRAP_RESAMPLES))
            # Equal-tailed percentile bootstrap for the arithmetic mean.
            low, high = means[249], means[9749]
        result.update(ci95_low=low, ci95_high=high)
    return result


describe = numeric_summary


def derived(table, row):
    row = dict(row)
    if table == 'scaling':
        row['delta_final_rss'] = float(row['final_rss']) - float(row['baseline_rss'])
    elif table == 'pressure':
        row['delta_rss'] = float(row['rss']) - float(row['baseline_rss'])
    elif table == 'footprint':
        for stage in ('live', 'freed', 'collected'):
            row[f'delta_{stage}_rss'] = float(row[f'{stage}_rss']) - float(row['baseline_rss'])
    return row


def metric_entries(table, rows, bootstrap=True):
    grouped = defaultdict(dict)
    fields = SCENARIO_FIELDS[table]
    for row in rows:
        key = tuple(str(row[field]) for field in fields)
        trial = int(row['trial'])
        if trial in grouped[key]:
            raise ValueError('Duplicate independent process trial')
        grouped[key][trial] = derived(table, row)
    entries = []
    for key, trials in sorted(grouped.items()):
        for metric in METRICS[table]:
            entries.append(dict(scenario=dict(zip(fields, key)), metric=metric,
                                **numeric_summary([trials[trial][metric] for trial in sorted(trials)], bootstrap)))
    return entries


def paired(rows, fields, metric, bootstrap=True, throughput=False):
    grouped = defaultdict(dict)
    for row in rows:
        key = tuple(str(row[field]) for field in fields)
        trial = int(row['trial'])
        paths = grouped[key].setdefault(trial, {})
        kind = 'native' if row['path'] == 'native' else 'api'
        if kind in paths:
            raise ValueError('Duplicate paired measurement')
        paths[kind] = float(row[metric])
    results = []
    for key, trials in sorted(grouped.items()):
        if any(set(paths) != {'native', 'api'} for paths in trials.values()):
            raise ValueError('Incomplete paired Native/API trials')
        ratios = []
        for trial in sorted(trials):
            native, api = trials[trial]['native'], trials[trial]['api']
            if min(native, api) <= 0:
                raise ValueError('Invalid paired timing')
            ratios.append(api / native if throughput else native / api)
        results.append(dict(scenario=dict(zip(fields, key)), metric=metric,
                            ratio='api/native' if throughput else 'native/api',
                            **numeric_summary(ratios, bootstrap)))
    return results


def summarize(rows_by_table, bootstrap=True):
    tables = {name: metric_entries(name, rows, bootstrap)
              for name, rows in rows_by_table.items()}
    comparisons = []
    for table, metric in (('latency', 'mean_ns'), ('scaling', 'operations_per_second'),
                          ('tails', 'allocate_p99_ns')):
        if table in rows_by_table:
            fields = tuple(field for field in SCENARIO_FIELDS[table] if field != 'path')
            comparisons.extend(dict(table=table, **entry) for entry in paired(
                rows_by_table[table], fields, metric, bootstrap, table == 'scaling'))
    return dict(schema=3, statistic='arithmetic mean of independent process trials',
                uncertainty='equal-tailed percentile bootstrap 95% CI of arithmetic mean',
                bootstrap_seed=BOOTSTRAP_SEED, bootstrap_resamples=BOOTSTRAP_RESAMPLES,
                outliers='All recorded trials retained without clipping or removal',
                tables=tables, paired=comparisons)


def write_summary(directory, native=False):
    directory = Path(directory)
    names = ('native',) if native else tuple(name for name in SCENARIO_FIELDS if name != 'native')
    rows = {}
    for name in names:
        path = directory / ('results.csv' if name == 'native' else f'{name}.csv')
        rows[name] = read_csv(path)
    (directory / 'summary.json').write_text(json.dumps(summarize(rows), indent=2) + '\n', encoding='utf-8')
