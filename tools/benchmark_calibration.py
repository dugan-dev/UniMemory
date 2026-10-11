"""Bounded sustained calibration; every pilot and failure remains evidence."""
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / 'ci'))
from publication_metadata import sanitize_publication

PROTOCOL = 'native-api-sustained-v4'
SUSTAINED_SECONDS = .125
TARGET_SECONDS = .35
PILOT_REPETITIONS = 3
PILOT_TIMEOUT_SECONDS = 30
CASE_BUDGET_SECONDS = 2
NORMAL_ROUND_LIMIT = 131072
OVERSUBSCRIBED_ROUND_LIMIT = 32
OPERATION_LIMIT = 100000000


def new_evidence(effective_cpus):
    return dict(schema=1, protocol=PROTOCOL,
                strategy='Per-profile paired Native/API counts; geometric sustained pilots and disclosed safety caps',
                sustained_target_seconds=SUSTAINED_SECONDS, measurement_target_seconds=TARGET_SECONDS,
                pilot_repetitions=PILOT_REPETITIONS, pilot_timeout_seconds=PILOT_TIMEOUT_SECONDS,
                predicted_case_budget_seconds=CASE_BUDGET_SECONDS,
                normal_scaling_round_limit=NORMAL_ROUND_LIMIT,
                oversubscribed_scaling_round_limit=OVERSUBSCRIBED_ROUND_LIMIT,
                effective_cpus=effective_cpus, records=[], decisions=[], failures=[])


def semantic_hash(evidence):
    encoded = json.dumps(sanitize_publication(evidence), sort_keys=True,
                         separators=(',', ':'), ensure_ascii=True).encode('utf-8')
    return hashlib.sha256(encoded).hexdigest()


def failure_record(error, **identity):
    def text(value):
        return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''
    return dict(identity, reason='timeout' if isinstance(error, subprocess.TimeoutExpired) else 'execution',
                timeout_seconds=getattr(error, 'timeout', None), returncode=getattr(error, 'returncode', None),
                stdout=text(getattr(error, 'stdout', getattr(error, 'output', ''))),
                stderr=text(getattr(error, 'stderr', '')), error_type=type(error).__name__)


def calibrate_pair(kind, case, paths, probe, *, initial, count_limit,
                   evidence, oversubscribed=False, save=lambda: None):
    count = min(initial, count_limit)
    durations = {}
    while True:
        durations = {path: [] for path in paths}
        for repeat in range(1, PILOT_REPETITIONS + 1):
            order = paths if repeat % 2 else tuple(reversed(paths))
            for path in order:
                identity = dict(kind=kind, case=case, path=path, count=count, repeat=repeat)
                try:
                    row = probe(path, count, repeat)
                    seconds = float(row['seconds'])
                    if not math.isfinite(seconds) or seconds <= 0:
                        raise ValueError('Nonpositive sustained pilot duration')
                except Exception as error:
                    evidence['failures'].append(failure_record(error, **identity))
                    save()
                    raise
                evidence['records'].append(dict(identity, row=row))
                durations[path].append(seconds)
        save()
        fastest = min(min(values) for values in durations.values())
        slowest = max(max(values) for values in durations.values())
        goal_met = fastest >= SUSTAINED_SECONDS
        if goal_met or count == count_limit or slowest >= CASE_BUDGET_SECONDS:
            break
        count = min(count * 2, count_limit)
    requested = max(count, math.ceil(count * TARGET_SECONDS / fastest))
    budget = max(1, math.floor(count * CASE_BUDGET_SECONDS / slowest))
    chosen = min(requested, count_limit, budget)
    estimates = {path: max(values) * chosen / count for path, values in durations.items()}
    decision = dict(kind=kind, case=case, count=chosen, count_limit=count_limit,
                    oversubscribed=oversubscribed, sustained_goal_met=goal_met,
                    capped=chosen < requested, estimated_seconds=estimates,
                    requested_count=requested, budget_count=budget,
                    pilot_seconds_min=fastest, pilot_seconds_max=slowest,
                    short_interval_expected=min(estimates.values()) < .25)
    evidence['decisions'].append(decision)
    save()
    return decision
