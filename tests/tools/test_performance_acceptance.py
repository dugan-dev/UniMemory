import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("acceptance", ROOT / "tools/ci/performance-acceptance.py")
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)

class PerformanceAcceptanceTests(unittest.TestCase):
    def test_rejects_statistics_debug_and_checks(self):
        profile = dict(statistics="OFF", configuration="Release", checks="AUTO")
        module.validate_profile(profile)
        for key, value in (("statistics", "ON"), ("configuration", "Debug"), ("checks", "ON")):
            with self.assertRaises(ValueError):
                module.validate_profile(dict(profile, **{key: value}))

    def test_slow_scenario_cannot_hide_behind_fast_average(self):
        self.assertFalse(module.accepted([100] * 10, [120] * 10, .9))
        self.assertTrue(module.accepted([100] * 10, [105] * 10, .9))

    def test_extreme_trial_is_retained(self):
        self.assertFalse(module.accepted([100] * 10, [100] * 9 + [1000], .9))

    def test_insufficient_and_invalid_samples_rejected(self):
        for native, api in (([100], [100]), ([100] * 10, [0] * 10), ([100] * 10, [100] * 9)):
            with self.assertRaises(ValueError):
                module.accepted(native, api, .9)

    def test_threshold_cannot_weaken_ninety_percent(self):
        for minimum in (.89, 0, 1.01, float("nan")):
            with self.assertRaises(ValueError):
                module.accepted([100] * 10, [100] * 10, minimum)
        self.assertTrue(module.accepted([90] * 10, [100] * 10, .9))
        process = subprocess.run([sys.executable, str(ROOT / "tools/ci/performance-acceptance.py"),
                                  "missing-executable", "unused-output", "--minimum", ".89"],
                                 capture_output=True, text=True)
        self.assertEqual(process.returncode, 2)
        self.assertIn("0.90..1.00", process.stderr)

    def test_instrumentation_rejected(self):
        profile = dict(statistics="OFF", configuration="Release", checks="AUTO")
        for parameters in (dict(sanitizer="address"), dict(coverage="ON"),
                           dict(compile_options="-O3;-fsanitize=address"), dict(configuration_flags="--coverage")):
            with self.assertRaises(ValueError):
                module.validate_profile(dict(profile, build_parameters=parameters))

    def test_stale_or_incompatible_compiled_profile_rejected(self):
        profile = dict(backend="standard", checks="AUTO")
        record = dict(configuration="Release", flags="-O3", performance_tests="ON")
        compiled = dict(record, actual_backend="standard", actual_statistics="OFF",
                        actual_checks="AUTO", actual_checks_enabled="0", actual_performance_tests="ON")
        module.validate_compiled_profile(profile, record, compiled)
        for key, value in (("flags", "-O2"), ("actual_statistics", "ON"),
                           ("actual_checks_enabled", "1"), ("actual_performance_tests", "OFF"),
                           ("actual_backend", "mimalloc"), ("configuration", "Debug")):
            with self.assertRaises(ValueError):
                module.validate_compiled_profile(profile, record, dict(compiled, **{key: value}))
        with self.assertRaises(ValueError):
            module.validate_compiled_profile(profile, record, {})

    def test_platform_affinity_is_truthful(self):
        self.assertEqual(module.affinity_for("Windows"), "single")
        self.assertEqual(module.affinity_for("Linux"), "single")
        self.assertEqual(module.affinity_for("Darwin"), "none")

    def test_all_calibration_samples_are_retained_with_unique_names(self):
        driver = Mock()
        times = iter((.01, .02, .4, .5))
        driver.execute.side_effect = lambda *args: ([dict(seconds=next(times))], "")
        driver.primary.side_effect = lambda rows: rows[0]
        retain = Mock()
        pair = (dict(variant="native"), dict(variant="api"))
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory)
            count, pilots = module.retained_calibration(driver, Path("probe"), pair, .35, raw, "raw-64", retain)
        self.assertEqual(pilots, 2)
        self.assertGreater(count, 10000)
        self.assertEqual(retain.call_count, 4)
        identities = [call.args[4] for call in retain.call_args_list]
        self.assertEqual(len(set(identities)), 4)
        self.assertTrue(all(call.args[0] == "calibration" for call in retain.call_args_list))
        self.assertTrue(all(call.args[3] is raw for call in driver.execute.call_args_list))

    def test_calibration_limit_fails_without_discarding_samples(self):
        driver = Mock()
        driver.execute.return_value = ([dict(seconds=.000001)], "")
        driver.primary.side_effect = lambda rows: rows[0]
        retain = Mock()
        with self.assertRaisesRegex(ValueError, "evidence retained"):
            module.retained_calibration(driver, Path("probe"), (dict(variant="native"), dict(variant="api")),
                                        .35, Path("raw"), "raw-64", retain)
        self.assertEqual(retain.call_count, driver.execute.call_count)
        self.assertEqual(max(call.args[2] for call in driver.execute.call_args_list), 200000000)

    def test_fast_paths_calibrate_above_fifty_million_with_equal_counts(self):
        driver = Mock()
        driver.execute.side_effect = lambda executable, case, count, raw, identity: ([dict(seconds=count * 4.63e-9)], "")
        driver.primary.side_effect = lambda rows: rows[0]
        retain = Mock()
        count, pilots = module.retained_calibration(driver, Path("probe"), (dict(variant="native"), dict(variant="api")),
                                                    .25, Path("raw"), "fast", retain)
        self.assertGreater(count, 50000000)
        self.assertLessEqual(count, 200000000)
        self.assertGreaterEqual(count * 4.63e-9, .25)
        self.assertEqual(retain.call_count, pilots * 2)
        calls = driver.execute.call_args_list
        for index in range(0, len(calls), 2):
            self.assertEqual(calls[index].args[2], calls[index + 1].args[2])
