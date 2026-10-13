import importlib.util
import json
import shutil
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
            with mock.patch.object(overview, "comparison_data", return_value={"source_revision": "b" * 40}):
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
            folder = ROOT / "docs/results/current/windows-x64" / backend / "OFF"
            rows = overview.read_csv(folder / "full.csv")
            expected = [overview.selected_median(rows, scenario, data["trials"], data["measurement_schema"] == 3)
                        for scenario in overview.SCENARIOS]
            self.assertEqual(values, expected)
        for name, heading in (("README.md", "## Performance"), ("README.zh-CN.md", "## \u6027\u80fd")):
            performance = (ROOT / name).read_text(encoding="utf-8").split(heading + "\n", 1)[1].split("\n## ", 1)[0]
            overview.validate_local_overview(ROOT, performance)
        self.assertEqual(len(data["platforms"]["windows"]["workloads"]), 8)
        self.assertEqual(len(data["platforms"]["windows"]["pressure"]), 30)

    def local_fixture(self, root):
        for name in ("README.md", "README.zh-CN.md", overview.LOCAL_SOURCE):
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        return {"source_revision": "a" * 40, "trials": 10, "measurement_schema": 3,
                "platforms": {"windows": {"overview": {backend: [100., 200., 300., 400.] for backend in overview.BACKENDS},
                                          "profiles": {backend: {"sweep_environment": {"measured_at_utc": "2026-10-13T00:00:00Z"},
                                                                "environment": {"compiler": {"CMAKE_CXX_COMPILER_VERSION": "19.44"}}}
                                                       for backend in overview.BACKENDS}}}}

    def test_local_table_wrong_value_rejected_against_local_snapshot(self):
        content = (ROOT / "README.md").read_text(encoding="utf-8")
        block = content.split("## Performance\n", 1)[1].split("\n## ", 1)[0]
        overview.validate_local_overview(ROOT, block)
        original = next(line for line in block.splitlines() if line.startswith("| Create and destroy"))
        cells = original.split("|")
        cells[2] = f" {float(cells[2]) + 1:.1f} "
        with self.assertRaisesRegex(ValueError, "differs from its snapshot"):
            overview.validate_local_overview(ROOT, block.replace(original, "|".join(cells)))
        with self.assertRaisesRegex(ValueError, "differs from its snapshot"):
            overview.validate_local_overview(ROOT, block.replace("256", "32"))

    def test_local_snapshot_retains_all_summaries_and_sanitized_provenance(self):
        snapshot = overview.read_json(ROOT / overview.LOCAL_SOURCE)
        self.assertEqual(len(snapshot["summary"]), 84)
        self.assertEqual(snapshot["trials"], 10)
        self.assertEqual(snapshot["sourceRoot"], ".")
        self.assertEqual(len(snapshot["source_hashes"]), 25)
        self.assertNotIn("Users/", json.dumps(snapshot))
        self.assertNotIn("Users\\\\", json.dumps(snapshot))
        for profile in snapshot["profiles"].values():
            self.assertEqual(profile["statistics"], "OFF")
            for binary in profile["sdk_binaries"].values():
                self.assertRegex(binary["sha256"], "^[0-9a-f]{64}$")
                self.assertNotIn("/", binary["filename"])
                self.assertNotIn("\\", binary["filename"])

    def test_refresh_preserves_local_labels_and_prose_but_updates_plot_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            data = self.local_fixture(root)
            before = {}
            for name in ("README.md", "README.zh-CN.md"):
                file = root / name
                text = file.read_text(encoding="utf-8").replace(overview.LOCAL_MARKER, overview.LOCAL_MARKER + "\nLocal guidance: 256 integers.")
                file.write_text(text, encoding="utf-8")
                before[name] = text
            output = root / "plot-input.json"
            with mock.patch.object(overview, "comparison_data", return_value=data):
                overview.refresh_overview(root, output, "a" * 40)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), data)
            for name, text in before.items():
                self.assertEqual((root / name).read_text(encoding="utf-8"), text)

    def test_invalid_local_table_prevents_any_refresh_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            data = self.local_fixture(root)
            file = root / "README.md"
            text = file.read_text(encoding="utf-8")
            original = next(line for line in text.splitlines() if line.startswith("| Create and destroy"))
            cells = original.split("|")
            cells[2] = f" {float(cells[2]) + 1:.1f} "
            file.write_text(text.replace(original, "|".join(cells)), encoding="utf-8")
            output = root / "plot-input.json"
            with mock.patch.object(overview, "comparison_data", return_value=data):
                with self.assertRaisesRegex(ValueError, "differs from its snapshot"):
                    overview.refresh_overview(root, output)
            self.assertFalse(output.exists())

    def test_canonical_ci_four_row_table_still_refreshes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            data = self.local_fixture(root)
            for name, heading, header in (("README.md", "## Performance", "| Operation |"),
                                          ("README.zh-CN.md", "## \u6027\u80fd", "| \u64cd\u4f5c |")):
                content = (heading + "\n\nWindows x64 \xb7 MSVC 19.43 \xb7 2026-01-01\n" + header + " Standard | mimalloc | jemalloc |\n"
                           "| --- | ---: | ---: | ---: |\n" + "".join(f"| Label {index} | 1.0 | 1.0 | 1.0 |\n" for index in range(4))
                           + "\nKeep existing guidance.\n\n## Usage\nKeep this too.\n")
                (root / name).write_text(content, encoding="utf-8")
            with mock.patch.object(overview, "comparison_data", return_value=data):
                overview.refresh_overview(root, root / "plot-input.json")
            for name in ("README.md", "README.zh-CN.md"):
                result = (root / name).read_text(encoding="utf-8")
                self.assertIn("| Label 1 | 200.0 | 200.0 | 200.0 |", result)
                self.assertIn("Keep existing guidance.", result)
                self.assertIn("2026-10-13", result)


if __name__ == "__main__":
    unittest.main()
