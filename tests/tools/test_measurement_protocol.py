import math
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))


class RepeatedMeasurements(unittest.TestCase):
    def test_sustained_calibration_bounds_oversubscribed_rounds_and_keeps_pilots(self):
        from benchmark_calibration import calibrate_pair, new_evidence
        evidence = new_evidence(3)
        def probe(path, count, repeat):
            return dict(seconds=count * .0001, operations=count * 512 * 8, checksum=count)
        decision = calibrate_pair('scaling', 'scaling:64:8', ('native', 'basic'), probe,
                                  initial=8, count_limit=32, oversubscribed=True, evidence=evidence)
        self.assertLessEqual(decision['count'], 32)
        self.assertTrue(decision['capped'])
        self.assertFalse(decision['sustained_goal_met'])
        self.assertTrue(decision['short_interval_expected'])
        self.assertEqual(len(evidence['records']), 18)
        self.assertEqual({row['path'] for row in evidence['records']}, {'native', 'basic'})

    def test_sustained_calibration_uses_one_paired_count_and_bounds_slow_path(self):
        from benchmark_calibration import calibrate_pair, new_evidence
        evidence = new_evidence(4)
        def probe(path, count, repeat):
            return dict(seconds=count * (.001 if path == 'native' else .02), operations=count,
                        checksum=count)
        decision = calibrate_pair('scaling', 'scaling:64:4', ('native', 'basic'), probe,
                                  initial=8, count_limit=1024, evidence=evidence)
        self.assertLessEqual(decision['estimated_seconds']['basic'], 2.0)
        self.assertTrue(decision['short_interval_expected'])
        self.assertTrue(evidence['records'])
        self.assertEqual({row['repeat'] for row in evidence['records']}, {1, 2, 3})

    def test_failed_pilot_is_recorded_before_failure_propagates(self):
        from benchmark_calibration import calibrate_pair, new_evidence
        import subprocess
        evidence = new_evidence(3)
        def probe(path, count, repeat):
            raise subprocess.TimeoutExpired(['benchmark'], 30, output='partial record', stderr='timeout')
        with self.assertRaises(subprocess.TimeoutExpired):
            calibrate_pair('scaling', 'scaling:64:8', ('native', 'basic'), probe,
                           initial=8, count_limit=32, evidence=evidence)
        self.assertEqual(evidence['failures'][0]['reason'], 'timeout')
        self.assertEqual(evidence['failures'][0]['stdout'], 'partial record')

    def test_six_profile_runner_keeps_calibration_separate_through_ten_rounds(self):
        import controlled_benchmarks as runner
        from benchmark_calibration import new_evidence
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / 'results'
            profiles = [(backend, mode) for backend in ('standard', 'mimalloc', 'jemalloc') for mode in ('OFF', 'ON')]
            pairs = {key: (root / f'{key[0]}-{key[1]}' / 'Delivery', root / f'{key[0]}-{key[1]}' / 'Sweep') for key in profiles}
            evidence = [dict(backend=backend, statistics=mode) for backend, mode in profiles]
            calls = []
            host = dict(effective_cpus=3, host_id='synthetic-test-only', affinity=dict(pinned=False, cpus=[], method='test'),
                        cpu_model='synthetic-test-only', platform='test', architecture='test', logical_cpus=3)
            def delivery(executable, directory, cpus):
                backend, mode = executable.parent.name.split('-')
                count = 1000 * (profiles.index((backend, mode)) + 1)
                calibration = dict(latency={'64': count}, scaling={'scaling:64:8': 32}, heap={})
                (directory / 'calibration-pilots.json').write_text(json.dumps(new_evidence(cpus)))
                return calibration
            def full(executable, directory, cpus):
                (directory / 'full-calibration.json').write_text(json.dumps(dict(schema=1, protocol=runner.PROTOCOL, records=[])))
                return {'raw:64:16:1:no': 64}
            def base(executable, directory, trials, offset, append, calibration, total, suite):
                selected = next(item for item in suite['profiles'] if executable.parent.name == item['backend'] + '-' + item['statistics'])
                self.assertEqual(calibration, selected['delivery_calibration'])
                self.assertNotIn('calibration', suite)
                calls.append((executable.parent.name, offset + 1, calibration['latency']['64']))
            def scales(executable, directory, trials, offset, append, calibration):
                self.assertEqual(calibration['scaling']['scaling:64:8'], 32)
            def sweep(executable, directory, trials, offset, append, operations, total, suite):
                self.assertEqual(Path(operations).read_text(), 'raw:64:16:1:no,64\n')
                self.assertEqual(total, 10)
            def selected_profile(executable):
                return {'sdk_runtime_files': {}}
            with contextlib.ExitStack() as stack:
                for owner, name, replacement in ((runner, 'executables', lambda _: (pairs, evidence)), (runner, 'control_host', lambda: host),
                    (runner, 'scaling_canary', lambda *args: None), (runner, 'calibrate_delivery', delivery),
                    (runner.sweep_driver, 'calibrate_sweep', full), (runner, 'run_base', base),
                    (runner, 'run_scaling_tails', scales), (runner.sweep_driver, 'run_sweep', sweep),
                    (runner, 'finalize_traces', lambda *args: None), (runner, 'write_summary', lambda *args: None),
                    (runner, 'read_profile', selected_profile), (runner.subprocess, 'run', lambda *args, **kwargs: None),
                    (runner.measurement_report, 'validate', lambda *args, **kwargs: None)):
                    stack.enter_context(mock.patch.object(owner, name, side_effect=replacement))
                stack.enter_context(mock.patch.object(sys, 'argv', ['controlled_benchmarks.py', str(root), str(output)]))
                stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                runner.main()
            self.assertEqual(len(calls), 60)
            for profile in pairs:
                items = [item for item in calls if item[0] == '-'.join(profile)]
                self.assertEqual({item[1] for item in items}, set(range(1, 11)))
                self.assertEqual(len({item[2] for item in items}), 1)
            self.assertEqual(len({item[2] for item in calls}), 6)

    def test_retains_extreme_trial_and_arithmetic_mean(self):
        from benchmark_statistics import describe
        result = describe([1] * 9 + [101])
        self.assertEqual(result['n'], 10)
        self.assertEqual((result['min'], result['max'], result['mean'], result['median']),
                         (1, 101, 11, 1))
        self.assertAlmostEqual(result['sample_sd'], math.sqrt(1000))
        self.assertEqual(result, describe([1] * 9 + [101]))
        self.assertLessEqual(result['ci95_low'], result['mean'])
        self.assertGreaterEqual(result['ci95_high'], result['mean'])

    def test_rejects_nonfinite_or_insufficient_samples(self):
        from benchmark_statistics import describe
        for values in ([], [1], [1, math.nan], [1, math.inf]):
            with self.assertRaises(ValueError):
                describe(values)

    def test_pairing_uses_trial_ratios(self):
        from benchmark_statistics import paired
        rows = [dict(trial=trial, backend='standard', path=path, bytes='64', mean_ns=value)
                for trial, native, api in ((1, 1, 2), (2, 9, 9))
                for path, value in (('native', native), ('disabled', api))]
        result = paired(rows, ('backend', 'bytes'), 'mean_ns')
        self.assertEqual(result[0]['mean'], .75)
        rows.pop()
        with self.assertRaisesRegex(ValueError, 'paired'):
            paired(rows, ('backend', 'bytes'), 'mean_ns')

    def test_order_varies_without_changing_coverage(self):
        from controlled_benchmarks import schedule
        profiles = [(backend, mode) for backend in ('standard', 'mimalloc', 'jemalloc')
                    for mode in ('OFF', 'ON')]
        result = schedule(profiles, 10, 42)
        self.assertEqual(result, schedule(profiles, 10, 42))
        rounds = [[(row['backend'], row['statistics']) for row in result if row['trial'] == trial]
                  for trial in range(1, 11)]
        self.assertGreater(len({tuple(items) for items in rounds}), 1)
        for items in rounds:
            self.assertEqual(set(items), set(profiles))
            self.assertEqual(len(items), 6)

    def test_shared_calibration_chooses_largest_equal_work(self):
        from controlled_benchmarks import merge_calibration
        self.assertEqual(merge_calibration([{'64': 100}, {'64': 400}, {'64': 250}]), {'64': 400})

    def test_common_host_build_rejects_different_crt_or_sdk_binary(self):
        import controlled_benchmarks as suite
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for backend in ('standard', 'mimalloc', 'jemalloc'):
                for mode in ('OFF', 'ON'):
                    directory = root / f'{backend}-{mode}'
                    directory.mkdir()
                    for name in ('UniMemoryDeliveryBenchmark', 'UniMemoryBenchmark'):
                        (directory / name).write_bytes(f'{backend}/{mode}/{name}'.encode())

            def profile(executable, altered=None):
                backend, mode = executable.parent.name.split('-')
                return dict(backend=backend, statistics=mode, configuration='Release', checks='AUTO',
                            build_parameters=dict(compiler='same compiler', msvc_runtime='DLL' if altered != 'crt' or mode == 'OFF' else 'static',
                                                  flags='', release_flags='same optimization', ipo='', compile_options=''),
                            sdk_binaries={} if backend == 'standard' else dict(
                                sdk_binary=dict(sha256=backend if altered != 'sdk' or mode == 'OFF' else 'different binary')))

            with mock.patch.object(suite, 'read_profile', side_effect=profile):
                pairs, evidence = suite.executables(root)
                self.assertEqual((len(pairs), len(evidence)), (6, 6))
            for factor, message in (('crt', 'Compiler, CRT'), ('sdk', 'same SDK binary')):
                with mock.patch.object(suite, 'read_profile', side_effect=lambda executable: profile(executable, factor)):
                    with self.assertRaisesRegex(ValueError, message):
                        suite.executables(root)

    def test_summary_accepts_lossless_compressed_empty_standard_heap(self):
        import csv
        import gzip
        import json
        from benchmark_statistics import METRICS, SCENARIO_FIELDS, write_summary
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for table, fields in SCENARIO_FIELDS.items():
                if table == 'native':
                    continue
                names = ['trial', *fields, *METRICS[table]]
                # Empty heap is legal for the standard SDK, including gzip-only form.
                if table == 'heap':
                    (root / 'heap.csv.gz').write_bytes(gzip.compress((','.join(names) + '\n').encode(), mtime=0))
                    continue
                extra = ['final_rss', 'baseline_rss'] if table == 'scaling' else ['baseline_rss', 'rss'] if table == 'pressure' else []
                names.extend(field for field in extra if field not in names)
                with (root / f'{table}.csv').open('w', newline='') as output:
                    writer = csv.DictWriter(output, names)
                    writer.writeheader()
                    for trial in (1, 2):
                        for path in ('native', 'disabled') if 'path' in fields else ('',):
                            row = dict.fromkeys(names, '1')
                            row.update(trial=trial)
                            if 'path' in fields:
                                row['path'] = path
                            writer.writerow(row)
            write_summary(root)
            result = json.loads((root / 'summary.json').read_text())
            self.assertEqual(result['tables']['heap'], [])


if __name__ == '__main__':
    unittest.main()
