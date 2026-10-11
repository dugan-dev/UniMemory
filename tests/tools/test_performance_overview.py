import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("overview", ROOT / "tools/ci/refresh-performance-overview.py")
overview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overview)


class OverviewTests(unittest.TestCase):
    def test_trusted_run_mismatch_rejects_all_artifacts_before_any_copy(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "repo"
            artifacts = Path(temporary) / "artifacts"
            for platform in ("linux-x64", "windows-x64", "macos-arm64"):
                for backend in overview.BACKENDS:
                    for mode in ("OFF", "ON"):
                        directory = artifacts / f"performance-{platform}-{backend}-{mode}" / "results"
                        directory.mkdir(parents=True)
                        run_id = "other" if (platform, backend, mode) == ("macos-arm64", "jemalloc", "ON") else "123"
                        (directory / "environment.json").write_text(json.dumps({"source_revision": "a" * 40, "run_id": run_id}), encoding="utf-8")
            fake = SimpleNamespace(validate=lambda path, **kwargs: json.loads((path / "environment.json").read_text(encoding="utf-8")))
            with mock.patch.object(overview, "load_tool", return_value=fake):
                with self.assertRaisesRegex(ValueError, "trusted run ID"):
                    overview.stage_artifacts(root, artifacts, "a" * 40, expected_run_id="123")
            self.assertFalse((root / "docs").exists())

    def test_repeated_mean_selects_per_process_means_and_retains_outlier(self):
        rows = [{"trial": str(trial), "workload": "create_destroy", "bytes": "64",
                 "alignment": "1", "threads": "1", "statistics": "disabled", "heap": "no",
                 "median_ns_per_operation": "999", "mean_ns_per_operation": str(value)}
                for trial, value in enumerate([1] * 9 + [100], 1)]
        self.assertEqual(overview.selected_median(rows, ("create_destroy", 64, 1, 1), 10, True), 10.9)
        for changed in (rows[:-1], rows[:-1] + [rows[0]]):
            with self.assertRaisesRegex(ValueError, "Incomplete"):
                overview.selected_median(changed, ("create_destroy", 64, 1, 1), 10, True)

    def test_three_trials_selected_without_heap_or_counter_results(self):
        def row(trial, value, **extra):
            return {"trial": str(trial), "workload": "create_destroy", "bytes": "64",
                    "alignment": "1", "threads": "1", "statistics": "disabled",
                    "heap": "no", "median_ns_per_operation": str(value), **extra}
        rows = [row(1, 7), row(2, 2), row(3, 4), row(1, 999, heap="yes"),
                row(2, 888, statistics="basic")]
        self.assertEqual(overview.selected_median(rows, ("create_destroy", 64, 1, 1)), 4)
        for invalid in (rows[:2], rows[:3] + [rows[0]],
                        [row(1, "nan"), row(2, 2), row(3, 4)]):
            with self.assertRaises(ValueError):
                overview.selected_median(invalid, ("create_destroy", 64, 1, 1))

    def test_stale_revision_rejected_before_any_overview_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "overview.json"
            with self.assertRaises(ValueError):
                overview.refresh_overview(ROOT, output, "a" * 40)
            self.assertFalse(output.exists())

    def test_current_overview_and_plot_use_exact_current_profile_values(self):
        data = overview.comparison_data(ROOT)
        for backend in overview.BACKENDS:
            profile = data["platforms"]["windows"]["profiles"][backend]
            self.assertEqual(profile["environment"]["source_revision"], data["source_revision"])
            self.assertEqual(profile["environment"]["build_profile"]["statistics"], "OFF")
            values = data["platforms"]["windows"]["overview"][backend]
            table = (ROOT / "README.md").read_text(encoding="utf-8")
            performance = table.split("## Performance\n", 1)[1].split("\n## ", 1)[0]
            for value in values:
                self.assertIn(f"{value:.1f}", performance)
        self.assertEqual(len(data["platforms"]["windows"]["workloads"]), 8)
        self.assertEqual(len(data["platforms"]["windows"]["pressure"]), 30)


if __name__ == "__main__":
    unittest.main()
