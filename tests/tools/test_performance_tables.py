import importlib.util
import json
from pathlib import Path
import re
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("tables", ROOT / "tools/ci/refresh-performance-tables.py")
tables = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tables)


class PerformanceTableTests(unittest.TestCase):
    def test_repeated_cells_show_mean_min_max_and_sample_cv(self):
        rows = [{"trial": str(trial), "_schema": 3, "_trials": 10,
                 "median_ns": "999", "mean_ns": str(value),
                 "rss": str((trial + value) * tables.MIB), "baseline_rss": str(trial * tables.MIB)}
                for trial, value in enumerate([1] * 9 + [100], 1)]
        self.assertEqual(tables.metric(rows, "median_ns"), 10.9)
        self.assertEqual(tables.resident_delta(rows, "rss"), 10.9)
        cell = tables.formatted_metric(rows, "median_ns")
        self.assertTrue(cell.startswith("10.9 [1.0, 100.0], CV "))
        self.assertIn("10.90 [1.00, 100.00]", tables.formatted_delta(rows, "rss"))
        for changed in (rows[:-1], rows[:-1] + [rows[0]]):
            with self.assertRaisesRegex(ValueError, "10 trials"):
                tables.metric(changed, "median_ns")

    def test_native_ten_process_matrix_uses_480_real_observations(self):
        rows = [{"trial": str(trial), "backend": backend, "workload": program,
                 "elapsed_seconds": str(100 if trial == 10 else 1), "peak_rss_kib": str(trial * 1024),
                 "user_seconds": "0", "system_seconds": "0", "exit_code": "0"}
                for trial in range(1, 11) for backend in tables.BACKENDS for program in tables.PROGRAMS]
        self.assertEqual(tables.native_summary(rows, 10, True)["cfrac", "mimalloc"], (10.9, 5.5))
        for changed in (rows[:-1], rows[:-1] + [rows[0]]):
            with self.assertRaisesRegex(ValueError, "480 unique"):
                tables.native_summary(changed, 10, True)

    def native_rows(self):
        return [{"trial": str(trial), "backend": backend, "workload": program,
                 "elapsed_seconds": str(trial), "peak_rss_kib": str(trial * 1024),
                 "user_seconds": "0", "system_seconds": "0", "exit_code": "0"}
                for trial in range(1, 4) for backend in tables.BACKENDS
                for program in tables.PROGRAMS]

    def test_baseline_subtracted_per_trial_before_median(self):
        rows = [{"trial": str(trial), "rss": str(rss * tables.MIB),
                 "baseline_rss": str(baseline * tables.MIB)}
                for trial, rss, baseline in ((1, 100, 99), (2, 110, 1), (3, 200, 100))]
        self.assertEqual(tables.resident_delta(rows, "rss"), 100)
        self.assertNotEqual(tables.resident_delta(rows, "rss"), 110 - 99)

    def test_complete_three_process_trials_required(self):
        rows = [{"trial": "1", "value": "1"}, {"trial": "1", "value": "2"},
                {"trial": "3", "value": "3"}]
        with self.assertRaisesRegex(ValueError, "3 trials"):
            tables.metric(rows, "value")

    def test_native_matrix_rejects_missing_duplicate_and_failed_runs(self):
        rows = self.native_rows()
        self.assertEqual(tables.native_summary(rows)["cfrac", "mimalloc"], (2, 2))
        for changed in (rows[:-1], rows[:-1] + [rows[0]]):
            with self.subTest(changed=changed[-1]):
                with self.assertRaisesRegex(ValueError, "144 unique"):
                    tables.native_summary(changed)
        rows[-1]["exit_code"] = "1"
        with self.assertRaisesRegex(ValueError, "failed"):
            tables.native_summary(rows)

    def test_native_input_and_sdk_profiles_remain_separate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "artifact"
            source.mkdir()
            import csv
            rows = self.native_rows()
            with (source / "results.csv").open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            (source / "environment.json").write_text(json.dumps({
                "trials": 3, "threads": 4, "commands": dict.fromkeys(tables.PROGRAMS, []),
                "interface": "native malloc/new through LD_PRELOAD", "revision": "b" * 40}), encoding="utf-8")
            sdk = {"source_revision": "a" * 40, "run_id": "1234",
                   "run_url": "https://github.com/example/repo/actions/runs/1234",
                   "measured_at_utc": "2026-10-11T00:00:00Z"}
            tables.import_native(root, source, sdk)
            retained = root / "docs/results/current/native-applications"
            manifest = tables.validate_native_data(retained)
            self.assertEqual(manifest["revision"], "b" * 40)
            self.assertEqual(manifest["source_revision"], "a" * 40)
            self.assertNotIn("build_profile", manifest)
            self.assertIn("2026-10-11", tables.source_line(manifest, native=True))

    def test_table_refresh_preserves_prose_and_translated_headers(self):
        source = "Before\n<!-- performance-source -->\nold\n<!-- /performance-source -->\n"
        source += "<!-- performance-table:stack -->\n| Platform | Bytes | ns/cycle |\n| old | old | old |\n<!-- /performance-table -->\nAfter\n"
        updated = tables.replace_blocks(source, {"stack": [["macOS", "64", "1.23"]]}, "new source")
        self.assertTrue(updated.startswith("Before\n"))
        self.assertTrue(updated.endswith("After\n"))
        self.assertIn("| Platform | Bytes | ns/cycle |", updated)
        self.assertIn("| macOS | 64 | 1.23 |", updated)
        self.assertEqual(updated, tables.replace_blocks(updated, {"stack": [["macOS", "64", "1.23"]]}, "new source"))
        with self.assertRaisesRegex(ValueError, "Missing or unexpected"):
            tables.replace_blocks(source, {"stack": [["macOS", "64", "1.23"]]}, "new source", expected={"stack", "cross-thread"})

    def test_current_profile_selection_and_public_tables(self):
        datasets, manifests = tables.sdk_inputs(ROOT)
        generated = tables.sdk_tables(datasets, manifests)
        self.assertEqual(len(generated["latency-windows-x64"]), 15)
        self.assertEqual(len(generated["workloads-macos-arm64"]), 8)
        self.assertEqual(len(generated["footprint"]), 15)
        self.assertEqual(len(generated["stack"]), 6)
        self.assertEqual(len(generated["pressure-linux-x64"][0]), 7)
        # A different process/profile supplies Basic. Never read OFF Native
        # or Heap Basic rows as the Global ON API measurement.
        row = next(row for row in generated["latency-windows-x64"]
                   if row[:2] == ["64", "mimalloc"])
        off = datasets["windows-x64", "mimalloc", "OFF"]["latency"]
        on = datasets["windows-x64", "mimalloc", "ON"]["latency"]
        self.assertEqual(row[2], f"{tables.metric(off, 'median_ns', backend='mimalloc', path='native', bytes=64):.1f}")
        self.assertEqual(row[3], f"{tables.metric(off, 'median_ns', backend='mimalloc', path='disabled', bytes=64):.1f}")
        self.assertEqual(row[4], f"{tables.metric(on, 'median_ns', backend='mimalloc', path='basic', bytes=64):.1f}")
        for platform, _ in tables.PLATFORMS:
            self.assertEqual(generated[f"pressure-{platform}"][0][2], f"{tables.resident_delta(datasets[platform, 'standard', 'OFF']['pressure'], 'rss', backend='standard', path='native', phase='dense'):.2f}")

    def test_chinese_labels_preserve_all_numeric_cells_and_english_input(self):
        datasets, manifests = tables.sdk_inputs(ROOT)
        generated = tables.sdk_tables(datasets, manifests)
        numeric_cells = lambda rows: [[cell for cell in row if re.fullmatch(r"-?\d+(?:\.\d+)?%?", cell)] for row in rows]
        for name, rows in generated.items():
            translated = tables.localized_rows(name, rows, "zh-CN")
            self.assertEqual(numeric_cells(translated), numeric_cells(rows), name)
            self.assertEqual(tables.localized_rows(name, rows, "en"), rows)
        labels = [row[0] for row in tables.localized_rows("workloads-windows-x64", generated["workloads-windows-x64"], "zh-CN")]
        self.assertTrue(all(re.search(r"[\u3400-\u9fff]", label) for label in labels[2:]))
        self.assertTrue(labels[0].startswith("make_unique"))
        self.assertTrue(labels[1].startswith("make_shared"))
        self.assertEqual(generated["workloads-windows-x64"][4][0], "Zeroed Block, 4 KiB")
        pressure_path = tables.localized_rows("pressure-linux-x64", generated["pressure-linux-x64"], "zh-CN")[0][1]
        rounding_path = tables.localized_rows("rounding", generated["rounding"], "zh-CN")[0][2]
        self.assertEqual(pressure_path, rounding_path)
        self.assertNotEqual(pressure_path, "Native")


if __name__ == "__main__":
    unittest.main()
