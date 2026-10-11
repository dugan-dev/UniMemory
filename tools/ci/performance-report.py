"""Validate complete measurements and render publication SVGs without dependencies."""
import argparse
import csv
from datetime import datetime, timedelta
from collections import defaultdict
from functools import lru_cache
import html
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from publication_metadata import sanitize_publication
import benchmark_statistics
from result_io import read_csv, read_json, resolve_path

BUILD_COMPARISON_FIELDS = ("compiler", "msvc_runtime", "flags", "release_flags", "ipo", "compile_options")
SUSTAINED_PROTOCOL = "native-api-sustained-v4"


def validate_timestamp(value):
    try:
        measured = datetime.fromisoformat(value)
        if measured.utcoffset() != timedelta(0):
            raise ValueError("Timestamp must record UTC")
        return measured
    except (TypeError, ValueError) as error:
        raise ValueError("Invalid UTC measurement timestamp") from error

BACKENDS = ("standard", "mimalloc", "jemalloc")
PATHS = ("native", "disabled", "basic")
THREADS = (1, 2, 4, 8, 16)
COLORS = {"standard": "#64748b", "mimalloc": "#0284c7", "jemalloc": "#d97706"}
LANGUAGES = ("en", "zh-CN")
CHARTS = ("throughput", "memory", "latency", "statistics", "retention", "workloads")
PLATFORM_LABELS = {"linux-x64": "Linux x64", "windows-x64": "Windows x64", "macos-arm64": "macOS"}
COMBINED_TABLES = ("scaling", "tails", "latency", "pressure", "full")
LOCALE_DIRECTORY = Path(__file__).resolve().parent / "locales"
API_SCENARIOS = (("raw", "64", "16"), ("zeroed", "64", "16"), ("reallocate", "64", "16"),
                 ("object", "64", "1"), ("shared_object", "64", "1"), ("std_vector", "256", "8"))


def sweep_scenarios(profile=None):
    """Pinned full-sweep contract, independent of measurements and their manifest."""
    expected = set()
    for backend in BACKENDS:
        heaps = ("no",) if backend == "standard" else ("no", "yes")
        for heap, tracking in itertools.product(heaps, ("disabled", "basic")):
            for workload, size, alignment in itertools.product(
                    ("raw", "zeroed", "reallocate", "reallocate_zeroed", "batch", "mixed_lifetime", "owned_block"),
                    (64,256,4096,65536), (16,64,256)):
                expected.add((backend, workload, str(size), str(alignment), "1", tracking, heap))
            for workload, size in itertools.product(("typed_raw", "array"), (64,256,4096,65536)):
                expected.add((backend, workload, str(size), "1", "1", tracking, heap))
            for workload in ("create_destroy", "object", "shared_object"):
                expected.add((backend, workload, "64", "1", "1", tracking, heap))
            for workload in ("std_vector", "pmr_vector"):
                expected.add((backend, workload, "256", "8", "1", tracking, heap))
        for heap, threads in itertools.product(heaps, (2,4,8)):
            expected.add((backend, "cross_thread", "64", "16", str(threads), "disabled", heap))
        if backend != "standard":
            for heap in heaps:
                expected.add((backend, "detailed_statistics", "0", "0", "1", "disabled", heap))
    for size, alignment in itertools.product((64,256,4096,65536), (16,64,256)):
        expected.add(("fixed_buffer", "stack_mark_rewind", str(size), str(alignment), "1", "disabled", "no"))
    if profile is not None:
        backend = profile['backend']
        mode = 'basic' if profile['statistics'] == 'ON' else 'disabled'
        expected = {(*case[:5], mode, case[6]) if case[6] == 'no' and
                    case[1] in ('cross_thread', 'detailed_statistics') else case for case in expected}
        expected = {case for case in expected if case[0] == 'fixed_buffer' or
                    (case[0] == backend and ((case[6] == 'no' and case[5] == mode) or
                     (case[6] == 'yes' and (profile['statistics'] == 'ON' or case[5] == 'disabled'))))}
    return expected


def dimensions(manifest):
    if manifest.get('schema') == 1:
        return BACKENDS, PATHS, 'disabled', None
    if manifest.get('schema') not in (2, 3):
        raise ValueError('Unsupported performance schema')
    profile = manifest.get('build_profile', {})
    backend = profile.get('backend')
    mode = 'basic' if profile.get('statistics') == 'ON' else 'disabled'
    if backend not in BACKENDS or profile.get('statistics') not in ('ON', 'OFF') or profile.get('core') != 'header-only':
        raise ValueError('Invalid selected build profile')
    if manifest.get('backends') != [backend] or manifest.get('paths') != ['native', mode]:
        raise ValueError('Measurement scope disagrees with its compiled profile')
    actual = manifest.get('allocator_environment', {})
    if actual.get('selected_backend') != backend or actual.get('selected_statistics') != mode or actual.get('core') != 'header-only':
        raise ValueError('Executable and recorded profile disagree')
    if backend != 'standard' and not profile.get('sdk_binaries'):
        raise ValueError('Missing actual SDK binary provenance')
    return (backend,), ('native', mode), mode, profile


def read(path):
    return read_csv(path)


def write_text(path, content):
    with path.open("w", encoding="utf-8", newline="\n") as file:
        file.write(content)


def trial_count(manifest):
    schema, count = manifest.get("schema"), manifest.get("trials")
    if type(count) is not int or (schema in (1, 2) and count != 3) or (schema == 3 and count < 10) or schema not in (1, 2, 3):
        raise ValueError("Unsupported or incomplete performance manifest")
    return count


def process_values(rows, value, trials):
    """One observation per fresh process; never discard a measured outlier."""
    keys = [int(row["trial"]) for row in rows]
    if len(keys) != trials or set(keys) != set(range(1, trials + 1)):
        raise ValueError(f"Expected one measurement from each of {trials} trials")
    values = [float(value(row)) for row in rows]
    if any(not math.isfinite(item) for item in values):
        raise ValueError("Non-finite process measurement")
    return values


def validate_controlled_suite(directory, manifest):
    suite = read_json(directory / "controlled-suite.json")
    if suite != manifest.get("controlled_suite"):
        raise ValueError("Controlled suite differs from recorded environment")
    count = trial_count(manifest)
    if (suite.get("schema") != 3 or suite.get("protocol") != manifest.get("measurement_protocol") or
            suite.get("trials") != count or not suite.get("host_id") or
            suite.get("cpu_model") != manifest.get("cpu_model")):
        raise ValueError("Invalid controlled host or protocol provenance")
    profiles = suite.get("profiles", [])
    expected = set(itertools.product(BACKENDS, ("OFF", "ON")))
    keys = [(item.get("backend"), item.get("statistics")) for item in profiles]
    if len(keys) != 6 or set(keys) != expected:
        raise ValueError("Incomplete controlled build profiles")
    schedule = [(item.get("trial"), item.get("backend"), item.get("statistics")) for item in suite.get("schedule", [])]
    if len(schedule) != count * 6 or set(schedule) != {(t, b, m) for t in range(1, count + 1) for b, m in expected}:
        raise ValueError("Missing or duplicate controlled process trials")
    profile = manifest["build_profile"]
    selected = next(item for item in profiles if (item["backend"], item["statistics"]) == (profile["backend"], profile["statistics"]))
    if (selected.get("executable_sha256") != manifest.get("executable_sha256") or
            not re.fullmatch(r"[0-9a-f]{64}", manifest.get("executable_sha256", "")) or
            profile.get("native_api_same_executable") is not True or profile.get("native_api_same_sdk_target") is not True):
        raise ValueError("Native/API executable or SDK target provenance differs")
    if any(selected.get(key) != profile.get("build_parameters", {}).get(key) for key in BUILD_COMPARISON_FIELDS):
        raise ValueError("Controlled build flags or CRT disagree with executable provenance")
    calibration = manifest.get("calibration", {})
    if manifest["measurement_protocol"] == SUSTAINED_PROTOCOL:
        if calibration != selected.get("delivery_calibration"):
            raise ValueError("Executable operation counts differ from its selected profile calibration")
        validate_calibration_pilots(directory, manifest, selected)
    elif (any(calibration.get(table) != suite.get("calibration", {}).get(table) for table in ("latency", "scaling")) or
          calibration.get("heap") != selected.get("heap_operation_counts")):
        raise ValueError("Executable operation counts differ from controlled calibration")
    affinity = suite.get("affinity", {})
    if (not isinstance(affinity.get("pinned"), bool) or not affinity.get("method") or
            not isinstance(affinity.get("cpus"), list) or (affinity["pinned"] and not affinity["cpus"])):
        raise ValueError("Missing controlled CPU affinity provenance")
    return suite


def canonical_json_sha(value):
    payload = json.dumps(sanitize_publication(value), sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def validate_calibration_pilots(directory, manifest, selected):
    evidence = read_json(directory / "calibration-pilots.json")
    if canonical_json_sha(evidence) != selected.get("calibration_sha256"):
        raise ValueError("Delivery calibration pilot hash differs from controlled provenance")
    constants = {"schema": 1, "protocol": SUSTAINED_PROTOCOL, "sustained_target_seconds": .125,
                 "measurement_target_seconds": .35, "pilot_repetitions": 3, "pilot_timeout_seconds": 30,
                 "predicted_case_budget_seconds": 2, "normal_scaling_round_limit": 131072,
                 "oversubscribed_scaling_round_limit": 32}
    if any(evidence.get(key) != value for key, value in constants.items()) or not evidence.get("strategy"):
        raise ValueError("Unsupported sustained calibration method or bounds")
    effective_cpus = manifest["controlled_suite"].get("effective_cpus", manifest["logical_cpus"])
    if evidence.get("effective_cpus") != effective_cpus:
        raise ValueError("Calibration CPU capacity differs from controlled host")
    calibration = manifest["calibration"]
    expected_cases = {"latency": {str(size) for size in (16, 64, 256, 4096, 65536)},
                      "scaling": {f"{workload}:{size}:{threads}" for workload, size, threads in itertools.product(("scaling", "handoff"), (64, 4096, 65536), THREADS)},
                      "heap": set() if manifest["build_profile"]["backend"] == "standard" else {"owns", "collect", "reset"}}
    if set(calibration) != set(expected_cases) or any(set(calibration[kind]) != cases for kind, cases in expected_cases.items()):
        raise ValueError("Missing or unexpected profile calibration workload")
    expected = {(kind, case): count for kind, cases in calibration.items() for case, count in cases.items()}
    decisions = evidence.get("decisions", [])
    keys = [(item.get("kind"), item.get("case")) for item in decisions]
    if len(keys) != len(expected) or set(keys) != set(expected):
        raise ValueError("Missing or duplicate delivery calibration decisions")
    for decision in decisions:
        count, limit = decision.get("count"), decision.get("count_limit")
        if (type(count) is not int or type(limit) is not int or not 0 < count <= limit or
                count != expected[decision["kind"], decision["case"]] or
                any(type(decision.get(key)) is not bool for key in ("oversubscribed", "sustained_goal_met", "capped", "short_interval_expected"))):
            raise ValueError("Invalid selected delivery calibration count or quality flags")
        if decision["kind"] == "scaling":
            if decision["oversubscribed"] != (int(decision["case"].split(":")[-1]) > effective_cpus):
                raise ValueError("Scaling oversubscription disclosure differs from controlled CPU capacity")
            cap = evidence["oversubscribed_scaling_round_limit" if decision["oversubscribed"] else "normal_scaling_round_limit"]
            if limit != cap:
                raise ValueError("Scaling calibration round limit differs from recorded bound")
        estimates = decision.get("estimated_seconds", {})
        if not estimates or any(not math.isfinite(float(value)) or float(value) <= 0 for value in estimates.values()):
            raise ValueError("Invalid sustained calibration duration estimate")
    records, failures = evidence.get("records", []), evidence.get("failures", [])
    if not isinstance(records, list) or not isinstance(failures, list):
        raise ValueError("Invalid retained calibration records or failures")
    seen, covered, groups = set(), set(), defaultdict(list)
    for record in records:
        kind, case, path, count, repeat = (record.get(key) for key in ("kind", "case", "path", "count", "repeat"))
        key = kind, case, path, count, repeat
        if key in seen or (kind, case) not in expected or type(count) is not int or count <= 0 or type(repeat) is not int:
            raise ValueError("Missing, duplicate or unexpected sustained calibration pilot")
        seen.add(key)
        covered.add((kind, case))
        groups[kind, case, path, count].append(record)
        row = record["row"]
        seconds, operations = float(row["seconds"]), int(row["operations"])
        if not math.isfinite(seconds) or seconds <= 0 or operations <= 0 or row["backend"] != manifest["build_profile"]["backend"]:
            raise ValueError("Invalid sustained calibration observation")
        if kind == "scaling":
            workload, size, threads = case.split(":")
            if (int(row["rounds"]) != count or operations != int(threads) * 512 * count or
                    row["path"] != path or row["bytes"] != size or row["threads"] != threads or
                    row["workload"] != ("same_thread" if workload == "scaling" else "handoff") or
                    not math.isclose(operations / seconds, float(row["operations_per_second"]), rel_tol=1e-5, abs_tol=1e-6)):
                raise ValueError("Scaling pilot count, workload or throughput disagrees with raw observation")
        elif (operations != count or
              (kind == "latency" and (row["path"] != path or row["bytes"] != case)) or
              (kind == "heap" and row["operation"] != case) or
              not math.isclose(float(row["mean_ns"]) * operations / 1e9, seconds, rel_tol=1e-5, abs_tol=1e-12)):
            raise ValueError("Latency/heap pilot count or elapsed interval disagrees with raw observation")
    if covered != set(expected):
        raise ValueError("Incomplete sustained calibration pilot coverage")
    for (kind, case, path, count), records in groups.items():
        if {item["repeat"] for item in records} != {1, 2, 3} or len(records) != 3:
            raise ValueError("Expected three retained repetitions of each sustained pilot")
    for kind, case in expected:
        paths = {key[2] for key in groups if key[:2] == (kind, case)}
        wanted = {"disabled"} if kind == "heap" else {"native", manifest["build_profile"]["api_path"]}
        if paths != wanted:
            raise ValueError("Missing paired Native/API calibration path")
        decision = next(item for item in decisions if (item["kind"], item["case"]) == (kind, case))
        oversubscribed = kind == "scaling" and int(case.split(":")[-1]) > effective_cpus
        limit = (32 if oversubscribed else 131072) if kind == "scaling" else 100000000
        if decision["count_limit"] != limit or decision["oversubscribed"] != oversubscribed:
            raise ValueError("Calibration safety bound differs from controlled CPU capacity")
        counts = sorted({key[3] for key in groups if key[:2] == (kind, case)})
        initial = {"latency": 20000, "scaling": 8, "heap": 2000}[kind]
        if counts[0] != min(initial, limit):
            raise ValueError("Sustained pilot progression has an invalid initial count")
        for index, count in enumerate(counts):
            if index and count != min(counts[index - 1] * 2, limit):
                raise ValueError("Sustained pilot counts must follow the retained geometric progression")
            if any((kind, case, path, count) not in groups for path in wanted):
                raise ValueError("Native/API sustained pilot counts differ within the profile")
            durations = {path: [float(item["row"]["seconds"]) for item in groups[kind, case, path, count]] for path in wanted}
            fastest = min(value for values in durations.values() for value in values)
            slowest = max(value for values in durations.values() for value in values)
            stopped = fastest >= .125 or count == limit or slowest >= 2
            if stopped != (index == len(counts) - 1):
                raise ValueError("Sustained pilot stopping decision disagrees with retained observations")
        requested = max(count, math.ceil(count * .35 / fastest))
        budget = max(1, math.floor(count * 2 / slowest))
        chosen = min(requested, limit, budget)
        estimates = {path: max(values) * chosen / count for path, values in durations.items()}
        if (decision["count"] != chosen or decision["sustained_goal_met"] != (fastest >= .125) or
                decision["capped"] != (chosen < requested) or
                decision["short_interval_expected"] != (min(estimates.values()) < .25) or
                set(decision["estimated_seconds"]) != set(estimates) or
                any(not math.isclose(float(decision["estimated_seconds"][path]), seconds, rel_tol=1e-10, abs_tol=1e-12) for path, seconds in estimates.items())):
            raise ValueError("Selected calibration count or quality flags disagree with all retained pilots")
        audit_fields = {"requested_count": requested, "budget_count": budget,
                        "pilot_seconds_min": fastest, "pilot_seconds_max": slowest}
        if any(key in decision and not math.isclose(float(decision[key]), value, rel_tol=1e-10, abs_tol=1e-12)
               for key, value in audit_fields.items()):
            raise ValueError("Calibration request, budget or duration extrema disagree with retained pilots")
    for failure in failures:
        if (failure.get("kind"), failure.get("case")) not in expected or not failure.get("reason"):
            raise ValueError("Invalid retained calibration failure provenance")
    return evidence


def validate_short_interval(row):
    if row.get("short_interval") != ("true" if float(row["seconds"]) < .25 else "false"):
        raise ValueError("Short measurement interval disclosure disagrees with elapsed time")


@lru_cache(maxsize=8192)
def bootstrap_ci(values):
    result = benchmark_statistics.numeric_summary(values, bootstrap=True)
    return result["ci95_low"], result["ci95_high"]


def validate_interval_rows(rows, name, manifest):
    suffix = "_per_operation" if name == "full" else ""
    fields = [prefix + "_ns" + suffix for prefix in ("min", "mean", "median", "max")]
    samples_field = "samples_ns" + suffix
    for row in rows:
        try:
            samples = [float(item) for item in row[samples_field].split(";")]
            values = [float(row[field]) for field in fields]
            operations = int(row["operations"])
            seconds = float(row["seconds"])
            if (not samples or any(not math.isfinite(item) or item <= 0 for item in samples + values) or
                    operations <= 0 or not math.isfinite(seconds) or seconds <= 0 or
                    not (values[0] <= values[1] <= values[3] and values[0] <= values[2] <= values[3])):
                raise ValueError(f"Invalid calibrated {name} interval")
            if not math.isclose(values[1] * operations / 1e9, seconds, rel_tol=1e-5, abs_tol=1e-12):
                raise ValueError(f"Calibrated {name} count, mean and elapsed interval disagree")
            if name in ("latency", "heap"):
                key = row["bytes"] if name == "latency" else row["operation"]
                if operations != manifest["calibration"][name][key]:
                    raise ValueError(f"Calibrated {name} operation count differs from controlled suite")
            if manifest.get("measurement_protocol") == SUSTAINED_PROTOCOL:
                validate_short_interval(row)
            expected = (min(samples), statistics.mean(samples), statistics.median(samples), max(samples))
            if any(not math.isclose(actual, wanted, rel_tol=1e-5, abs_tol=1e-6) for actual, wanted in zip(values, expected)):
                raise ValueError(f"Recorded {name} statistics disagree with raw interval samples")
        except KeyError as error:
            raise ValueError(f"Invalid {name} interval schema") from error


def validate_sweep_provenance(directory, manifest, sweep_manifest, rows):
    validate_timestamp(sweep_manifest.get("measured_at_utc"))
    profile = manifest["build_profile"]
    selected = next(item for item in manifest["controlled_suite"]["profiles"] if
                    (item["backend"], item["statistics"]) == (profile["backend"], profile["statistics"]))
    if (sweep_manifest.get("schema") != 3 or sweep_manifest.get("measurement_protocol") != manifest["measurement_protocol"] or
            sweep_manifest.get("repetitions") != 1 or sweep_manifest.get("sequential_processes") is not True or
            any(str(sweep_manifest.get(key)) != str(manifest.get(key)) for key in ("source_revision", "run_id", "run_url")) or
            sweep_manifest.get("executable_sha256") != selected.get("sweep_executable_sha256") or
            sweep_manifest.get("benchmark_source_sha256") != selected.get("sweep_source_sha256") or
            not re.fullmatch(r"[0-9a-f]{64}", sweep_manifest.get("executable_sha256", "")) or
            not re.fullmatch(r"[0-9a-f]{64}", sweep_manifest.get("benchmark_source_sha256", ""))):
        raise ValueError("Full sweep executable, source or protocol provenance differs from controlled suite")
    path = directory / "sweep-operations.txt"
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != sweep_manifest.get("operations_file_sha256") or digest != selected.get("full_operations_sha256"):
        raise ValueError("Full sweep calibration hash differs from retained operation counts")
    counts = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        key, count = line.split(",")
        if key in counts or not count.isdigit() or int(count) <= 0:
            raise ValueError("Missing or duplicate full sweep calibration counts")
        counts[key] = int(count)
    keys = {":".join(row[field] for field in ("workload", "bytes", "alignment", "threads", "heap")) for row in rows}
    if set(counts) != keys:
        raise ValueError("Full sweep calibration scenario coverage differs")
    if any(int(row["operations"]) != counts[":".join(row[field] for field in ("workload", "bytes", "alignment", "threads", "heap"))] for row in rows):
        raise ValueError("Full sweep used different operation counts than recorded calibration")
    if manifest["measurement_protocol"] == SUSTAINED_PROTOCOL:
        validate_full_calibration(directory, manifest, selected, sweep_manifest, counts)


def validate_full_calibration(directory, manifest, selected, sweep_manifest, counts):
    evidence = read_json(directory / "full-calibration.json")
    digest = canonical_json_sha(evidence)
    if digest != selected.get("full_calibration_sha256") or digest != sweep_manifest.get("full_calibration_sha256"):
        raise ValueError("Full calibration pilot hash differs from controlled provenance")
    effective_cpus = manifest["controlled_suite"].get("effective_cpus", manifest["logical_cpus"])
    if (evidence.get("schema") != 1 or evidence.get("protocol") != SUSTAINED_PROTOCOL or evidence.get("counts") != counts or
            evidence.get("effective_cpus") != effective_cpus):
        raise ValueError("Full calibration method or selected counts disagree")
    records = evidence.get("records", [])
    fields = ("backend", "workload", "bytes", "alignment", "threads", "statistics", "heap")
    keys = [tuple(row[field] for field in fields) for row in records]
    expected = sweep_scenarios(manifest["build_profile"])
    if len(keys) != len(expected) or set(keys) != expected:
        raise ValueError("Missing or duplicate full calibration scenario")
    grouped = defaultdict(list)
    for record in records:
        key = ":".join(record[field] for field in ("workload", "bytes", "alignment", "threads", "heap"))
        count, limit = int(record["operations"]), int(record["count_limit"])
        history_counts = [int(value) for value in record["pilot_operations"].split(";")]
        history_seconds = [float(value) for value in record["pilot_seconds"].split(";")]
        if (not 0 < count <= limit or not history_counts or len(history_counts) != len(history_seconds) or
                len(history_counts) % 3 or any(value <= 0 for value in history_counts) or
                any(not math.isfinite(value) or value <= 0 for value in history_seconds)):
            raise ValueError("Invalid retained full calibration repetitions or bound")
        if any(record.get(field) not in ("true", "false") for field in ("sustained_goal_met", "capped", "oversubscribed")):
            raise ValueError("Missing full calibration quality disclosure")
        estimated = float(record["estimated_seconds"])
        if not math.isfinite(estimated) or estimated <= 0:
            raise ValueError("Invalid full calibration duration estimate")
        oversubscribed = int(record["threads"]) > effective_cpus
        expected_limit = 8192 * 32 if record["workload"] == "cross_thread" and oversubscribed else 100000000
        if limit != expected_limit or record["oversubscribed"] != str(oversubscribed).lower():
            raise ValueError("Full calibration safety bound differs from controlled CPU capacity")
        previous = None
        for offset in range(0, len(history_counts), 3):
            pilot_count = history_counts[offset]
            if len(set(history_counts[offset:offset + 3])) != 1 or pilot_count != (256 if previous is None else min(previous * 2, limit)):
                raise ValueError("Full sustained pilot counts must retain all three repetitions and geometric progression")
            seconds = history_seconds[offset:offset + 3]
            fastest, slowest = min(seconds), max(seconds)
            stopped = fastest >= .125 or pilot_count == limit or slowest >= 2
            if stopped != (offset == len(history_counts) - 3):
                raise ValueError("Full sustained pilot stopping decision disagrees with retained observations")
            previous = pilot_count
        desired = max(pilot_count, int(pilot_count * .35 / fastest) + 1)
        budget = max(64, int(pilot_count * 2 / slowest) // 64 * 64)
        chosen = min((desired + 63) // 64 * 64, limit, budget)
        if (count != chosen or record["sustained_goal_met"] != str(fastest >= .125).lower() or
                record["capped"] != str(chosen < desired).lower() or
                not math.isclose(estimated, slowest * chosen / pilot_count, rel_tol=1e-10, abs_tol=1e-12)):
            raise ValueError("Full calibration chosen count or quality flags disagree with all retained pilots")
        grouped[key].append(count)
    if any(counts[key] != min(values) for key, values in grouped.items()):
        raise ValueError("Full calibration shared scenario counts must preserve the smallest bounded choice")


def validate_tail_traces(directory, manifest, strict_artifact=False):
    """Verify bulk traces in artifacts; retain hash/count/quantile provenance."""
    if manifest.get("trace_storage") != {"location": "workflow artifacts", "retention_days": 90}:
        raise ValueError("Missing tail trace artifact storage provenance")
    rows = read(directory / "tails.csv")
    expected = {f"raw/{row['trial']}-{row['backend']}-{row['path']}-{row['bytes']}.csv.gz": row for row in rows}
    evidence = manifest.get("tail_trace_evidence", {})
    if set(evidence) != set(expected) or any(item.get("samples") != 8192 or not re.fullmatch(r"[0-9a-f]{64}", item.get("sha256", "")) for item in evidence.values()):
        raise ValueError("Missing or duplicate tail trace hash/count provenance")
    metrics = ("allocate_p50_ns", "allocate_p95_ns", "allocate_p99_ns", "allocate_p999_ns", "allocate_max_ns", "free_p99_ns", "clock_p50_ns")
    ledger = {"schema": 1, "source_revision": manifest["source_revision"], "run_id": str(manifest["run_id"]),
              "method": "Nearest-rank allocation/free quantiles; clock median; all 8192 operation samples retained in workflow artifact", "traces": []}
    if strict_artifact:
        actual = {str(path.relative_to(directory)).replace("\\", "/") for path in (directory / "raw").glob("*.csv.gz")}
        if actual != set(expected):
            raise ValueError("Missing or unexpected operation tail traces")
        fields = {"backend", "path", "bytes", "sample", "allocate_ns", "free_ns", "clock_ns"}
        for name, row in sorted(expected.items()):
            path = directory / name
            if hashlib.sha256(path.read_bytes()).hexdigest() != evidence[name]["sha256"]:
                raise ValueError("Operation tail trace hash differs from measured provenance")
            samples = read(path)
            if (len(samples) != 8192 or any(set(sample) != fields for sample in samples) or
                    {int(sample["sample"]) for sample in samples} != set(range(8192)) or
                    any(any(sample[key] != row[key] for key in ("backend", "path", "bytes")) for sample in samples)):
                raise ValueError("Missing, duplicate or unexpected operation tail samples")
            values = {key: [float(sample[key]) for sample in samples] for key in ("allocate_ns", "free_ns", "clock_ns")}
            if any(not math.isfinite(value) or value < 0 for group in values.values() for value in group):
                raise ValueError("Invalid individual-operation tail sample")
            quantile = lambda items, fraction: sorted(items)[math.ceil(len(items) * fraction) - 1]
            quantiles = dict(zip(metrics, (quantile(values["allocate_ns"], .5), quantile(values["allocate_ns"], .95),
                                          quantile(values["allocate_ns"], .99), quantile(values["allocate_ns"], .999),
                                          max(values["allocate_ns"]), quantile(values["free_ns"], .99), statistics.median(values["clock_ns"]))))
            if any(not math.isclose(float(row[key]), value, rel_tol=1e-10, abs_tol=1e-10) for key, value in quantiles.items()):
                raise ValueError("Tail quantiles disagree with retained individual-operation traces")
            ledger["traces"].append({"path": name, **evidence[name], "quantiles": quantiles})
        return ledger
    published = read_json(directory / "tail-traces.json")
    if any(published.get(key) != value for key, value in ledger.items() if key != "traces"):
        raise ValueError("Tail trace ledger source or method differs from measured provenance")
    records = published.get("traces", [])
    names = [item.get("path") for item in records]
    if len(names) != len(expected) or set(names) != set(expected):
        raise ValueError("Missing or duplicate published tail trace provenance")
    for record in records:
        name = record["path"]
        if any(record.get(key) != value for key, value in evidence[name].items()):
            raise ValueError("Published tail trace hash/count provenance differs")
        if set(record.get("quantiles", {})) != set(metrics) or any(not math.isclose(float(record["quantiles"][key]), float(expected[name][key]), rel_tol=1e-10, abs_tol=1e-10) for key in metrics):
            raise ValueError("Published tail quantiles disagree with the validated trace ledger")
    return published


def validate_summary(directory, native=False):
    names = ("native",) if native else tuple(name for name in benchmark_statistics.SCENARIO_FIELDS if name != "native")
    rows = {name: read(directory / ("results.csv" if name == "native" else f"{name}.csv")) for name in names}
    expected = benchmark_statistics.summarize(rows, bootstrap=False)
    for table, entries in expected["tables"].items():
        fields = benchmark_statistics.SCENARIO_FIELDS[table]
        grouped = defaultdict(dict)
        for row in rows[table]:
            grouped[tuple(row[field] for field in fields)][int(row["trial"])] = benchmark_statistics.derived(table, row)
        for entry in entries:
            trials = grouped[tuple(entry["scenario"][field] for field in fields)]
            values = tuple(float(trials[trial][entry["metric"]]) for trial in sorted(trials))
            entry["ci95_low"], entry["ci95_high"] = bootstrap_ci(values)
    for entry in expected["paired"]:
        table = entry["table"]
        grouped = defaultdict(dict)
        for row in rows[table]:
            if all(row[field] == value for field, value in entry["scenario"].items()):
                grouped[int(row["trial"])]["native" if row["path"] == "native" else "api"] = float(row[entry["metric"]])
        values = tuple(pair["api"] / pair["native"] if entry["ratio"] == "api/native" else pair["native"] / pair["api"]
                       for _, pair in sorted(grouped.items()))
        entry["ci95_low"], entry["ci95_high"] = bootstrap_ci(values)
    actual = read_json(directory / "summary.json")
    for field in ("schema", "statistic", "uncertainty", "bootstrap_seed", "bootstrap_resamples", "outliers"):
        if actual.get(field) != expected[field]:
            raise ValueError("Invalid repeated-measurement statistical method")

    def check_entries(measured, calculated):
        if len(measured) != len(calculated):
            raise ValueError("Missing or duplicate statistical summary scenarios")
        for recorded, wanted in zip(measured, calculated):
            for key, value in wanted.items():
                if isinstance(value, (int, float)) and not isinstance(value, bool):
                    result = recorded.get(key)
                    if not isinstance(result, (int, float)) or not math.isfinite(result) or not math.isclose(result, value, rel_tol=1e-10, abs_tol=1e-10):
                        raise ValueError("Statistical summary disagrees with raw process observations")
                elif recorded.get(key) != value:
                    raise ValueError("Statistical summary disagrees with raw process observations")
            low, high = recorded.get("ci95_low"), recorded.get("ci95_high")
            if (not isinstance(low, (int, float)) or not isinstance(high, (int, float)) or
                    not math.isfinite(low) or not math.isfinite(high) or
                    low > high or
                    (low < wanted["min"] and not math.isclose(low, wanted["min"], rel_tol=1e-12, abs_tol=1e-12)) or
                    (high > wanted["max"] and not math.isclose(high, wanted["max"], rel_tol=1e-12, abs_tol=1e-12))):
                raise ValueError("Invalid bootstrap confidence interval")

    if set(actual.get("tables", {})) != set(expected["tables"]):
        raise ValueError("Incomplete repeated-measurement statistical tables")
    for name, calculated in expected["tables"].items():
        check_entries(actual["tables"][name], calculated)
    check_entries(actual.get("paired", []), expected["paired"])
    return actual


def validate(directory, strict_artifact=False):
    directory = Path(directory)
    manifest = read_json(directory / "environment.json")
    count = trial_count(manifest)
    trials_range = range(1, count + 1)
    backends, paths, api_path, profile = dimensions(manifest)
    if manifest["schema"] == 3:
        validate_timestamp(manifest.get("measured_at_utc"))
        validate_controlled_suite(directory, manifest)
    for name, expected, key in (
        ("scaling", set(itertools.product(trials_range, backends, paths, ("same_thread", "handoff"), (64, 4096, 65536), THREADS)),
         lambda row: (int(row["trial"]), row["backend"], row["path"], row["workload"], int(row["bytes"]), int(row["threads"]))),
        ("tails", set(itertools.product(trials_range, backends, paths, (16, 64, 256, 4096, 65536))),
         lambda row: (int(row["trial"]), row["backend"], row["path"], int(row["bytes"]))),
    ):
        rows = read(directory / f"{name}.csv")
        keys = [key(row) for row in rows]
        if len(keys) != len(expected) or set(keys) != expected:
            raise ValueError(f"Missing, duplicate or unexpected {name} scenarios")
        metric = "operations_per_second" if name == "scaling" else "allocate_p99_ns"
        if any(not math.isfinite(float(row[metric])) or float(row[metric]) <= 0 for row in rows):
            raise ValueError(f"Invalid {name} measurement")
        if name == "tails" and any(int(row["samples"]) != 8192 for row in rows):
            raise ValueError("Incomplete operation samples")
        numeric = ("operations", "seconds", "baseline_rss", "final_rss", "peak_rss") if name == "scaling" else (
            "allocate_p50_ns", "allocate_p95_ns", "allocate_p999_ns", "allocate_max_ns", "free_p99_ns", "clock_p50_ns")
        try:
            if any(not math.isfinite(float(row[field])) or float(row[field]) < 0 for row in rows for field in numeric):
                raise ValueError(f"Invalid {name} numeric field")
            if name == "scaling" and any(int(row["operations"]) != int(row["threads"])*(512*int(row["rounds"]) if manifest["schema"] == 3 else 4096) or
                                          float(row["seconds"]) <= 0 or float(row["peak_rss"]) <= 0 for row in rows):
                raise ValueError("Invalid scaling operation count or duration")
            if name == "scaling" and manifest["schema"] == 3:
                for row in rows:
                    workload = "scaling" if row["workload"] == "same_thread" else "handoff"
                    rounds = manifest["calibration"]["scaling"][f"{workload}:{row['bytes']}:{row['threads']}"]
                    if (int(row["rounds"]) != rounds or rounds <= 0 or
                            not math.isclose(float(row["operations"]) / float(row["seconds"]), float(row["operations_per_second"]), rel_tol=1e-5, abs_tol=1e-6)):
                        raise ValueError("Scaling count, duration or throughput differs from controlled calibration")
                    if manifest["measurement_protocol"] == SUSTAINED_PROTOCOL:
                        validate_short_interval(row)
            if name == "tails" and any(not (float(row["allocate_p50_ns"]) <= float(row["allocate_p95_ns"]) <=
                                            float(row["allocate_p99_ns"]) <= float(row["allocate_p999_ns"]) <=
                                            float(row["allocate_max_ns"])) for row in rows):
                raise ValueError("Invalid percentile ordering")
        except KeyError as error:
            raise ValueError(f"Invalid {name} schema") from error
    if not manifest.get("source_revision") or manifest["source_revision"] == "unrecorded":
        raise ValueError("Missing source provenance")
    matrices = (
        ("latency", set(itertools.product(trials_range, backends, paths, (16,64,256,4096,65536))),
         ("trial", "backend", "path", "bytes"), ("min_ns", "median_ns", "max_ns")),
        ("pressure", set(itertools.product(trials_range, backends, paths,
                                           ("dense", "sparse", "churn_dense", "churn_sparse", "freed"))),
         ("trial", "backend", "path", "phase"), ("baseline_rss", "rss", "peak_rss", "requested_bytes", "live_blocks")),
        ("footprint", {(trial, backend, stats, heap) for trial in trials_range for backend in backends
                       for stats in ('disabled', 'basic') for heap in (('no',) if backend == 'standard' else ('no', 'yes'))
                       if profile is None or ((heap == 'no' and stats == api_path) or
                          (heap == 'yes' and (profile['statistics'] == 'ON' or stats == 'disabled')))},
         ("trial", "backend", "statistics", "heap"), ("baseline_rss", "live_rss", "freed_rss", "collected_rss", "peak_rss")),
        ("heap", set(itertools.product(trials_range, tuple(b for b in backends if b != "standard"), ("owns", "collect", "reset"))),
         ("trial", "backend", "operation"), ("median_ns", "operations")),
    )
    for name, expected, fields, metrics in matrices:
        rows = read(directory / f"{name}.csv")
        try:
            keys = [tuple(int(row[field]) if field in ("trial", "bytes") else row[field] for field in fields) for row in rows]
            if len(keys) != len(expected) or set(keys) != expected:
                raise ValueError(f"Missing, duplicate or unexpected {name} scenarios")
            if any(not math.isfinite(float(row[field])) or float(row[field]) < 0 for row in rows for field in metrics):
                raise ValueError(f"Invalid {name} measurement")
            if name == "pressure" and any(row["phase"] == "freed" and (int(row["live_blocks"]) or int(row["requested_bytes"])) for row in rows):
                raise ValueError("Pressure workload retained outstanding requests")
        except KeyError as error:
            raise ValueError(f"Invalid {name} schema") from error
        if manifest["schema"] == 3 and name in ("latency", "heap"):
            validate_interval_rows(rows, name, manifest)
    sweep = read(directory / "full.csv")
    sweep_manifest = read_json(directory / "sweep-environment.json")
    scenario_fields = ("backend", "workload", "bytes", "alignment", "threads", "statistics", "heap")
    trials = {trial: [] for trial in trials_range}
    for row in sweep:
        trial = int(row["trial"])
        if trial not in trials or not math.isfinite(float(row["median_ns_per_operation"])) or float(row["median_ns_per_operation"]) <= 0:
            raise ValueError("Invalid full-sweep measurement")
        trials[trial].append(tuple(row[field] for field in scenario_fields))
    expected = sweep_scenarios(profile)
    for keys in trials.values():
        if len(keys) != len(expected) or set(keys) != expected or len(keys) != sweep_manifest["scenarios_per_trial"]:
            raise ValueError("Incomplete full sweep")
    for backend, (workload, size, alignment) in itertools.product(backends, API_SCENARIOS):
        if (backend, workload, size, alignment, "1", api_path, "no") not in expected:
            raise ValueError("Missing representative API workload")
    if profile is not None and sweep_manifest.get('build_profile') != profile:
        raise ValueError('Full sweep and matched benchmark build profiles differ')
    if manifest["schema"] == 3:
        validate_interval_rows(sweep, "full", manifest)
        if sweep_manifest.get("trials") != count:
            raise ValueError("Full sweep trial count differs from controlled measurements")
        validate_sweep_provenance(directory, manifest, sweep_manifest, sweep)
        validate_summary(directory)
        ledger = validate_tail_traces(directory, manifest, strict_artifact)
        if strict_artifact:
            manifest["_validated_tail_ledger"] = ledger
    return manifest


@lru_cache(maxsize=1)
def chinese_catalog():
    return read_json(LOCALE_DIRECTORY / "zh-CN.json")


def localized(text, language):
    if language == "en":
        return text
    if language != "zh-CN":
        raise ValueError("Unsupported chart language")
    return chinese_catalog()[text]


def chart(title, ylabel, xs, series, caption, bars=False, xlabel="", language="en", metadata=None, trials=3, mean=False):
    width, height = 1100, 560
    left, top, plot_width, plot_height = 92, 92, 760, 350
    maximum = max(value for _, _, values in series for value in values) * 1.12
    if maximum <= 0:
        raise ValueError("Chart requires positive measurements")
    title, ylabel = localized(title, language), localized(ylabel, language)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" xml:lang="{language}" viewBox="0 0 {width} {height}" role="img">',
             f'<title>{html.escape(title)}</title>', '<rect width="1100" height="560" fill="#ffffff"/>',
             '<g font-family="system-ui,Segoe UI,Microsoft YaHei,PingFang SC,Noto Sans CJK SC,sans-serif" fill="#0f172a">',
             f'<text x="92" y="40" font-size="25" font-weight="650">{html.escape(title)}</text>',
             f'<text x="92" y="66" font-size="14" fill="#475569">{html.escape(ylabel)}</text>']
    if metadata is not None:
        parts.insert(2, '<metadata>' + html.escape(json.dumps(sanitize_publication(metadata), sort_keys=True)) + '</metadata>')
    if bars:
        parts.append('<defs>')
        for backend, color in COLORS.items():
            parts.append(f'<pattern id="{backend}-basic" width="6" height="6" patternUnits="userSpaceOnUse">'
                         f'<rect width="6" height="6" fill="{color}"/>'
                         '<path d="M0,6L6,0" stroke="#ffffff" stroke-opacity="0.6" stroke-width="1.2"/></pattern>')
        parts.append('</defs>')
    for tick in range(6):
        y = top + plot_height * (1 - tick / 5)
        parts.extend([f'<path d="M{left},{y}h{plot_width}" stroke="#e2e8f0"/>',
                      f'<text x="80" y="{y+5}" text-anchor="end" font-size="13">{maximum*tick/5:.2g}</text>'])
    for index, value in enumerate(xs):
        x = left + plot_width * (index + .5) / len(xs)
        label = localized(value, language) if isinstance(value, str) else str(value)
        parts.append(f'<text x="{x}" y="465" text-anchor="middle" font-size="14">{html.escape(label)}</text>')
    for number, (backend, path, values) in enumerate(series):
        color = COLORS[backend]
        fill = f'url(#{backend}-basic)' if path == "basic" else color
        opacity = .45 if path == "native" else .9
        points = []
        for index, value in enumerate(values):
            x = left + plot_width * (index + .5) / len(xs)
            y = top + plot_height * (1 - value / maximum)
            if bars:
                bar_width = plot_width / len(xs) / (len(series) + 2)
                x += (number - (len(series)-1)/2) * bar_width
                parts.append(f'<rect data-path="{path}" x="{x-bar_width*.42}" y="{y}" width="{bar_width*.84}" height="{top+plot_height-y}" fill="{fill}" opacity="{opacity}"/>')
            else:
                points.append(f"{x},{y}")
                parts.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{color}"/>')
        if not bars:
            dash = ' stroke-dasharray="7 5"' if path == "native" else ''
            parts.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2.5"{dash}/>')
        label = f"{'Standard' if backend == 'standard' else backend} / {localized('Native' if path == 'native' else 'API + stats' if path == 'basic' else 'UniMemory', language)}"
        if bars:
            parts.append(f'<rect data-path="{path}" x="880" y="{110+number*34}" width="28" height="8" fill="{fill}" opacity="{opacity}"/>')
        else:
            parts.append(f'<path d="M880,{115+number*34}h28" stroke="{color}" stroke-width="3"'+ (' stroke-dasharray="7 5"' if path == 'native' else '') + '/>')
        parts.append(f'<text x="880" y="{133+number*34}" font-size="12">{label}</text>')
    if xlabel:
        parts.append(f'<text x="472" y="487" text-anchor="middle" font-size="13">{html.escape(localized(xlabel, language))}</text>')
    footer = localized("{trials} fresh-process trials; arithmetic mean shown. Min/max and variability: raw summaries." if mean else
                       "{trials} legacy fresh-process trials; median shown. See raw data for scope and environment.", language).format(trials=trials)
    parts.extend([f'<text x="92" y="505" font-size="13" fill="#475569">{html.escape(caption[:145])}</text>',
                  f'<text x="92" y="528" font-size="12" fill="#64748b">{html.escape(footer)}</text>', '</g></svg>'])
    return "\n".join(parts) + "\n"


def aggregate(rows, key, metric, mean=False):
    groups = defaultdict(list)
    for row in rows:
        groups[key(row)].append(float(row[metric]))
    reducer = statistics.mean if mean else statistics.median
    return {key: reducer(values) for key, values in groups.items()}


def validate_profile_group(manifests):
    """Reject a mixed host or SDK before presenting a controlled comparison."""
    manifests = list(manifests)
    contracts = {(item["source_revision"], item.get("measurement_protocol"), item.get("pressure_protocol"),
                  item["trials"], item["schema"], str(item.get("run_id"))) for item in manifests}
    profiles = {(item["build_profile"]["backend"], item["build_profile"]["statistics"]): item for item in manifests}
    if len(manifests) != 6 or set(profiles) != set(itertools.product(BACKENDS, ("OFF", "ON"))):
        raise ValueError("Incomplete controlled build profile group")
    if len(contracts) != 1:
        raise ValueError("Performance profiles have different source revisions, protocols, trial counts or run IDs")
    if manifests[0]["schema"] != 3:
        return
    flags = [tuple(item["build_profile"].get("build_parameters", {}).get(key) for key in BUILD_COMPARISON_FIELDS) for item in manifests]
    if any(record != flags[0] for record in flags[1:]):
        raise ValueError("Controlled performance profiles have different compiler, CRT or optimization flags")
    if any(item["controlled_suite"] != manifests[0]["controlled_suite"] for item in manifests[1:]):
        raise ValueError("Performance profiles were not measured on one controlled host and schedule")
    hardware = ("cpu_model", "logical_cpus", "architecture", "compiler")
    if any(any(item.get(key) != manifests[0].get(key) for key in hardware) for item in manifests[1:]):
        raise ValueError("Controlled performance profiles have different hardware or compiler provenance")
    for backend in BACKENDS:
        off, on = (profiles[backend, mode]["build_profile"] for mode in ("OFF", "ON"))
        hashes = lambda profile: {key: item.get("sha256") for key, item in profile.get("sdk_binaries", {}).items()}
        if hashes(off) != hashes(on):
            raise ValueError("Controlled ON/OFF profiles use different SDK binaries")


def load_combined(directory, expected_revision=None):
    """Validate six independent profiles before selecting OFF baselines and ON stats.

    This is a presentation dataset, not a synthetic benchmark manifest: executable,
    CPU and SDK provenance remain attached to each original measured profile.
    """
    directory = Path(directory)
    profiles, tables = {}, {name: [] for name in COMBINED_TABLES}
    contract = None
    for backend, mode in itertools.product(BACKENDS, ("OFF", "ON")):
        profile_directory = directory / backend / mode
        if not (profile_directory / "environment.json").is_file():
            raise ValueError(f"Missing performance profile: {directory.name}/{backend}/{mode}")
        manifest = validate(profile_directory)
        if manifest["schema"] not in (2, 3) or manifest["build_profile"]["backend"] != backend or manifest["build_profile"]["statistics"] != mode:
            raise ValueError(f"Unexpected performance profile: {directory.name}/{backend}/{mode}")
        revision = manifest["source_revision"]
        if expected_revision is not None and revision != expected_revision:
            raise ValueError(f"Stale source revision in {directory.name}/{backend}/{mode}")
        profile_contract = (revision, manifest.get("measurement_protocol"), manifest.get("pressure_protocol"), manifest["trials"], manifest["schema"])
        if not all(profile_contract[:3]):
            raise ValueError("Missing shared measurement protocol")
        if contract is not None and profile_contract != contract:
            raise ValueError("Performance profiles have different source revisions, protocols or trial counts")
        contract = profile_contract
        profiles[f"{backend}/{mode}"] = {
            "source": f"docs/results/current/{directory.name}/{backend}/{mode}/environment.json",
            "manifest": manifest,
            "sweep_source": f"docs/results/current/{directory.name}/{backend}/{mode}/sweep-environment.json",
            "sweep_manifest": read_json(profile_directory / "sweep-environment.json"),
            "data_sources": {name: f"docs/results/current/{directory.name}/{backend}/{mode}/{resolve_path(profile_directory / (name + '.csv')).name}" for name in COMBINED_TABLES},
            **({"summary_source": f"docs/results/current/{directory.name}/{backend}/{mode}/{resolve_path(profile_directory / 'summary.json').name}",
                "controlled_source": f"docs/results/current/{directory.name}/{backend}/{mode}/{resolve_path(profile_directory / 'controlled-suite.json').name}",
                "trace_source": f"docs/results/current/{directory.name}/{backend}/{mode}/{resolve_path(profile_directory / 'tail-traces.json').name}"} if manifest["schema"] == 3 else {}),
            **({"calibration_source": f"docs/results/current/{directory.name}/{backend}/{mode}/{resolve_path(profile_directory / 'calibration-pilots.json').name}",
                "full_calibration_source": f"docs/results/current/{directory.name}/{backend}/{mode}/{resolve_path(profile_directory / 'full-calibration.json').name}"} if manifest.get("measurement_protocol") == SUSTAINED_PROTOCOL else {}),
        }
        for name in COMBINED_TABLES:
            if mode == "OFF":
                tables[name].extend(read(profile_directory / f"{name}.csv"))
            elif name == "tails":
                tables[name].extend(row for row in read(profile_directory / "tails.csv") if row["path"] == "basic")
    validate_profile_group(item["manifest"] for item in profiles.values())
    return {
        "tables": tables,
        "provenance": {"measurement_schema": contract[4], "source_revision": contract[0],
                       "measurement_protocol": contract[1], "pressure_protocol": contract[2], "trials": contract[3],
                       "platform": directory.name, "profiles": profiles,
                       "comparison_scope": ("One controlled host per platform; all profiles share the recorded schedule and SDK binaries. Statistics ON includes feature cost."
                                            if contract[4] == 3 else "Independent profile builds/runners; hardware and SDK binaries may differ. Descriptive comparison, not causal overhead."),
                       "calibration_scope": ("Native/API counts match within each profile; SDK/statistics profiles may use different bounded counts. Time and throughput are normalized per operation."
                                             if contract[1] == SUSTAINED_PROTOCOL else
                                             "Recorded shared Delivery counts; full sweep and heap use profile-specific calibration." if contract[4] == 3 else
                                             "Legacy fixed-count trials on independent profiles; hardware and SDK binaries may differ."),
                       "path_profiles": {"native": "OFF", "disabled": "OFF", "basic": "ON"}},
    }


def render_combined(combined, output, label, language):
    render_charts(None, output, label, combined["provenance"], language, combined=combined)


def compact_provenance(provenance):
    value = {key: item for key, item in provenance.items() if key != "profiles"}
    value["profiles"] = {key: {field: item for field, item in source.items() if field.endswith("source")}
                         for key, source in provenance["profiles"].items()}
    value["runs"] = sorted({source["manifest"].get("run_url", "") for source in provenance["profiles"].values()})
    return value


def render_combined_analysis(combined, output, label):
    provenance = combined["provenance"]
    rows = aggregate(combined["tables"]["latency"],
                     lambda row: (row["backend"], row["path"], int(row["bytes"])), "median_ns")
    run_urls = sorted({profile["manifest"]["run_url"] for profile in provenance["profiles"].values()})
    links = " \xb7 ".join(f"[remote run]({url})" for url in run_urls)
    analysis = [f"# Performance \xb7 {label}\n",
                f"Source: `{provenance['source_revision']}` \xb7 {links}\n",
                "Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. "
                "Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.\n",
                "| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |\n| --- | ---: | ---: | ---: | ---: |"]
    for backend, size in itertools.product(BACKENDS, (16, 64, 256, 4096, 65536)):
        native, api = rows[backend, "native", size], rows[backend, "disabled", size]
        analysis.append(f"| {backend} | {size} | {native:.2f} | {api:.2f} | {api/native:.3f} |")
    analysis.extend(["\nTail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. "
                     "Thread counts above each profile's logical CPU count are oversubscribed.\n",
                     "Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). "
                     "The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.\n"])
    write_text(output / "README.md", "\n".join(analysis).rstrip() + "\n")
    write_text(output / "summary.json", json.dumps(sanitize_publication(provenance), indent=2) + "\n")


def refresh_reports(current_root, image_root, expected_revision=None, profile_charts=False):
    """Refresh all publication charts only after the whole source matrix validates."""
    current_root, image_root = Path(current_root), Path(image_root)
    datasets = {platform: load_combined(current_root / platform, expected_revision) for platform in PLATFORM_LABELS}
    contracts = {(data["provenance"]["source_revision"], data["provenance"]["measurement_protocol"],
                  data["provenance"]["pressure_protocol"], data["provenance"]["trials"]) for data in datasets.values()}
    if len(contracts) != 1:
        raise ValueError("Platforms have different source revisions, protocols or trial counts")
    for platform, data in datasets.items():
        platform_output = image_root / platform
        platform_output.mkdir(parents=True, exist_ok=True)
        for language in LANGUAGES:
            render_combined(data, platform_output, PLATFORM_LABELS[platform], language)
        write_text(platform_output / "summary.json", json.dumps(sanitize_publication(compact_provenance(data["provenance"])), indent=2) + "\n")
        if not profile_charts:
            continue
        for key, source in data["provenance"]["profiles"].items():
            output = platform_output / key
            output.mkdir(parents=True, exist_ok=True)
            for language in LANGUAGES:
                render_charts(current_root / platform / key, output,
                              f"{PLATFORM_LABELS[platform]} \xb7 {key}", source["manifest"], language)
    return {"source_revision": next(iter(contracts))[0], "profiles": 18, "charts": 252 if profile_charts else 36}


def render_charts(directory, output, label, manifest, language, combined=None):
    if combined is None:
        backends, paths, api_path, profile = dimensions(manifest)
        read_rows = lambda name: read(directory / f"{name}.csv")
        provenance = {"measurement_schema": manifest["schema"], "source_revision": manifest["source_revision"],
                      "profiles": {label: {"manifest": manifest,
                                           "sweep_manifest": read_json(directory / "sweep-environment.json")}}}
    else:
        backends, paths, api_path, profile = BACKENDS, PATHS, "disabled", None
        read_rows = lambda name: combined["tables"][name]
        provenance = combined["provenance"]
    mean = provenance["measurement_schema"] == 3
    trials = provenance.get("trials", manifest.get("trials", 3))
    reduced = lambda rows, key, metric: aggregate(rows, key, metric, mean=mean)

    def caption(template):
        if combined is not None:
            if mean:
                if template == "{platform} \xb7 {revision} \xb7 Native, API, and API + statistics":
                    template = "{platform} \xb7 {revision} \xb7 one controlled host \xb7 OFF baselines; ON includes statistics feature cost"
                elif template == "{platform} \xb7 {revision} \xb7 API only, statistics disabled \xb7 workload operations differ; compare within each group":
                    template = "{platform} \xb7 {revision} \xb7 Standard = 1 \xb7 one controlled host \xb7 compare within each workload"
                else:
                    template = "{platform} \xb7 {revision} \xb7 one controlled host \xb7 arithmetic mean of fresh-process trials"
            elif template == "{platform} \xb7 {revision} \xb7 Native, API, and API + statistics":
                template = "{platform} \xb7 {revision} \xb7 Native/API: OFF; API + stats: ON \xb7 independent builds/runners; no causal cost"
            elif template == "{platform} \xb7 {revision} \xb7 API only, statistics disabled \xb7 workload operations differ; compare within each group":
                template = "{platform} \xb7 {revision} \xb7 Standard = 1 \xb7 independent OFF builds/runners; not a controlled ranking"
            else:
                template = "{platform} \xb7 {revision} \xb7 OFF baselines \xb7 independent builds/runners; hardware and SDKs may differ"
        cpus = manifest.get("controlled_suite", {}).get("effective_cpus", manifest.get("logical_cpus", ""))
        return localized(template, language).format(platform=label, cpus=cpus, revision=manifest["source_revision"][:12])

    def write(name, title, ylabel, xs, series, description, xlabel, bars=False):
        suffix = "" if language == "en" else ".zh-CN"
        sources = {path: "ON" if path == "basic" else "OFF" for _, path, _ in series} if combined else None
        # Keep exact plotted values and references; full hardware/SDK manifests
        # already live beside the raw CSV and need not be copied into 36 SVGs.
        compact = compact_provenance(provenance)
        if combined is None:
            compact["profiles"] = {label: {"source": "environment.json", "sweep_source": "sweep-environment.json"}}
        metadata = {**compact, "chart": name, "x_values": list(xs),
                    "data_file": {"throughput": "scaling.csv", "memory": "scaling.csv", "latency": "tails.csv",
                                  "statistics": "tails.csv", "retention": "pressure.csv", "workloads": "full.csv"}[name],
                    "aggregation": "arithmetic mean of fresh-process trial observations" if mean else "legacy median of fresh-process trial observations",
                    "metric": {"throughput": "operations_per_second", "memory": "peak_rss", "latency": "allocate_p99_ns",
                               "statistics": "allocate_p99_ns", "retention": "rss_minus_baseline_rss",
                               "workloads": "mean_ns_per_operation" if mean else "median_ns_per_operation"}[name],
                    "unit": {"throughput": "million allocation/free pairs per second", "memory": "MiB", "latency": "ns",
                             "statistics": "ns", "retention": "MiB", "workloads": "ratio to Standard" if combined else "ns"}[name],
                    **({"metric": "rss_minus_baseline_rss", "unit": "MiB",
                        "aggregation": "arithmetic mean of per-trial RSS deltas" if mean else "median of per-trial RSS deltas"} if name == "retention" else {}),
                    "series": [{"backend": backend, "path": path, "values": values,
                                **({"profile": f"{backend}/{sources[path]}", "data_source": provenance["profiles"][f"{backend}/{sources[path]}"]["data_sources"][{"throughput": "scaling", "memory": "scaling", "latency": "tails", "statistics": "tails", "retention": "pressure", "workloads": "full"}[name]]} if sources else {})}
                               for backend, path, values in series]}
        content = chart(title, ylabel, xs, series, caption(description), bars=bars, xlabel=xlabel, language=language, metadata=metadata, trials=trials, mean=mean)
        write_text(output / f"{name}{suffix}.svg", content)

    rows = read_rows("scaling")
    series = [(backend, path) for backend in backends for path in ("native", api_path)]
    scaling_caption = "{platform} \xb7 {cpus} logical CPUs \xb7 {revision} \xb7 64 B cross-thread workload"
    filtered = [row for row in rows if row["workload"] == "handoff" and int(row["bytes"]) == 64]
    for file, metric, title, ylabel, factor in (
        ("throughput", "operations_per_second", "Cross-thread throughput", "Million allocation/free pairs per second \xb7 higher is better", 1e6),
        ("memory", "peak_rss", "Peak resident memory", "MiB \xb7 lower is better", 1024**2),
    ):
        values = reduced(filtered, lambda row: (row["backend"], row["path"], int(row["threads"])), metric)
        write(file, title, ylabel, THREADS, [(backend, path, [values[backend, path, threads]/factor for threads in THREADS]) for backend, path in series], scaling_caption, "Threads")
    tail_values = reduced(read_rows("tails"), lambda row: (row["backend"], row["path"], int(row["bytes"])), "allocate_p99_ns")
    sizes = (16, 64, 256, 4096, 65536)
    write("latency", "Individual allocation p99", "Nanoseconds \xb7 includes measured clock overhead \xb7 lower is better", sizes,
        [(backend, path, [tail_values[backend, path, size] for size in sizes]) for backend, path in series],
        "{platform} \xb7 {revision} \xb7 individual operation samples, not batch averages", "Request size (B)", bars=True)
    write("statistics", "Statistics cost \xb7 allocation p99", "Nanoseconds \xb7 lower is better", sizes,
        [(backend, path, [tail_values[backend, path, size] for size in sizes]) for backend in backends for path in paths],
        ("{platform} \xb7 {revision} \xb7 Native, API, and API + statistics" if profile is None else
         "{platform} \xb7 {revision} \xb7 selected API statistics mode \xb7 compare within each workload"), "Request size (B)", bars=True)
    phases = ("dense", "sparse", "churn_dense", "churn_sparse", "freed")
    pressure = [{**row, "resident_delta": float(row["rss"]) - float(row["baseline_rss"])} for row in read_rows("pressure")]
    memory = reduced(pressure, lambda row: (row["backend"], row["path"], row["phase"]), "resident_delta")
    write("retention", "Mixed-lifetime memory retention", "Resident MiB above baseline \xb7 not a fragmentation or leak rate", phases,
        [(backend, path, [memory[backend, path, phase]/1024**2 for phase in phases]) for backend, path in series],
        "{platform} \xb7 {revision} \xb7 16,384 slots; eight refill/free cycles; final phase has zero live requests", "Memory phase")
    workloads = tuple(item[0] for item in API_SCENARIOS)
    sweep = [row for row in read_rows("full") if (row["workload"], row["bytes"], row["alignment"]) in API_SCENARIOS and row["threads"] == "1" and
             row["statistics"] == api_path and row["heap"] == "no"]
    workload_values = reduced(sweep, lambda row: (row["backend"], row["workload"]), "mean_ns_per_operation" if mean else "median_ns_per_operation")
    if profile is None:
        write("workloads", "API workloads \xb7 normalized time", "Relative to UniMemory Standard = 1 \xb7 lower is better", workloads,
            [(backend, "disabled", [workload_values[backend, workload]/workload_values["standard", workload] for workload in workloads]) for backend in backends],
            "{platform} \xb7 {revision} \xb7 API only, statistics disabled \xb7 workload operations differ; compare within each group", "Workload", bars=True)
    else:
        write("workloads", "API workloads \xb7 absolute time", "Nanoseconds per workload operation \xb7 lower is better", workloads,
            [(backend, api_path, [workload_values[backend, workload] for workload in workloads]) for backend in backends],
            "{platform} \xb7 {revision} \xb7 selected API statistics mode \xb7 compare within each workload", "Workload", bars=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--label", required=True)
    parser.add_argument("--charts-only", action="store_true", help="Regenerate charts from existing data without rewriting analysis or baseline signals")
    parser.add_argument("--strict-artifact", action="store_true", help="Verify retained operation traces before publication")
    args = parser.parse_args()
    manifest = validate(args.input, strict_artifact=args.strict_artifact)
    backends, paths, api_path, profile = dimensions(manifest)
    args.output.mkdir(parents=True, exist_ok=True)
    for language in LANGUAGES:
        render_charts(args.input, args.output, args.label, manifest, language)
    if args.charts_only:
        return
    sizes = (16, 64, 256, 4096, 65536)
    mean = manifest["schema"] == 3
    field = "mean_ns" if mean else "median_ns"
    latency = aggregate(read(args.input / "latency.csv"), lambda row: (row["backend"], row["path"], int(row["bytes"])), field, mean=mean)
    analysis = [f"# Performance \xb7 {args.label}\n", f"Source: `{manifest['source_revision']}` \xb7 [remote run]({manifest['run_url']})\n",
                "Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.\n",
                "| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |\n| --- | ---: | ---: | ---: | ---: |"]
    for backend in backends:
        for size in sizes:
            native, api = latency[backend, "native", size], latency[backend, api_path, size]
            analysis.append(f"| {backend} | {size} | {native:.2f} | {api:.2f} | {api/native:.3f} |")
    if profile is not None:
        analysis.append(f"\nProfile: `{profile['backend']}` / statistics `{profile['statistics']}` / checks `{profile['checks']}`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.\n")
    analysis.extend(["\n## Interpretation\n", "Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.\n"])
    previous = Path(__file__).resolve().parents[2] / "docs/results/current" / args.label
    previous_manifest = previous / "environment.json"
    if previous_manifest.exists():
        prior = read_json(previous_manifest)
        comparable = all(sanitize_publication(prior.get(key)) == sanitize_publication(manifest.get(key)) for key in
                         ("compiler", "cpu_model", "logical_cpus", "measurement_protocol", "benchmark_sha256", "build_profile"))
        if comparable:
            old = aggregate(read(previous / "latency.csv"), lambda row: (row["backend"], row["path"], int(row["bytes"])), field, mean=mean)
            analysis.append("\n## Baseline signals\n\nSame recorded CPU/compiler/protocol. These signals require repeated confirmation.\n\n| Backend | Size | API/Native ratio change |\n| --- | ---: | ---: |")
            for backend in backends:
                for size in sizes:
                    before = old[backend, api_path, size] / old[backend, "native", size]
                    now = latency[backend, api_path, size] / latency[backend, "native", size]
                    analysis.append(f"| {backend} | {size} | {(now/before-1)*100:+.1f}% |")
        else:
            analysis.append("\nBaseline comparison withheld: recorded hardware, compiler or measurement protocol differs.\n")
    else:
        analysis.append("\nFirst recorded baseline; no regression comparison is available yet.\n")
    write_text(args.output / "README.md", "\n".join(analysis).rstrip() + "\n")
    summary = {"source_revision": manifest["source_revision"], "run_url": manifest["run_url"],
               "scope": args.label, "logical_cpus": manifest["logical_cpus"],
               "effective_cpus": manifest.get("controlled_suite", {}).get("effective_cpus", manifest["logical_cpus"]),
               "oversubscribed_threads": [threads for threads in THREADS if threads > manifest.get("controlled_suite", {}).get("effective_cpus", manifest["logical_cpus"])],
               "performance_gate": manifest["performance_gate"], "tail_method": manifest["tail_method"]}
    if mean:
        summary.update(validate_summary(args.input))
    summary.update(trials=manifest["trials"], aggregation="mean" if mean else "legacy median")
    write_text(args.output / "summary.json", json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
