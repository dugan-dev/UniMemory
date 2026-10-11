import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("overview", ROOT / "tools/ci/refresh-performance-overview.py")
overview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overview)


class OverviewTests(unittest.TestCase):
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
            table = (ROOT / "README.md").read_text()
            performance = table.split("## Performance\n", 1)[1].split("\n## ", 1)[0]
            for value in values:
                self.assertIn(f"{value:.1f}", performance)
        self.assertEqual(len(data["platforms"]["windows"]["workloads"]), 8)
        self.assertEqual(len(data["platforms"]["windows"]["pressure"]), 30)


if __name__ == "__main__":
    unittest.main()
