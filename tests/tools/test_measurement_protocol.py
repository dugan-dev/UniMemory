import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))


class RepeatedMeasurements(unittest.TestCase):
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
