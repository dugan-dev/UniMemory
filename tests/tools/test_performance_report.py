import importlib.util
import itertools
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("report", ROOT / "tools/ci/performance-report.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
spec = importlib.util.spec_from_file_location("measurement", ROOT / "tools/ci/performance.py")
measurement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measurement)


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        (self.directory / "environment.json").write_text(json.dumps({"schema": 1, "trials": 3, "source_revision": "a"*40}))
        scaling = [{"trial": trial, "backend": backend, "path": path, "workload": workload,
                    "bytes": size, "threads": threads, "operations_per_second": 1,
                    "operations": threads*4096, "seconds": 1, "baseline_rss": 100, "final_rss": 100, "peak_rss": 100}
                   for trial, backend, path, workload, size, threads in itertools.product(
                       range(1, 4), report.BACKENDS, report.PATHS, ("same_thread", "handoff"), (64, 4096, 65536), report.THREADS)]
        tails = [{"trial": trial, "backend": backend, "path": path, "bytes": size,
                  "samples": 8192, "allocate_p50_ns": 1, "allocate_p95_ns": 1, "allocate_p99_ns": 1,
                  "allocate_p999_ns": 1, "allocate_max_ns": 1, "free_p99_ns": 1, "clock_p50_ns": 1}
                 for trial, backend, path, size in itertools.product(range(1, 4), report.BACKENDS, report.PATHS, (16,64,256,4096,65536))]
        measurement.write_rows(self.directory / "scaling.csv", scaling)
        measurement.write_rows(self.directory / "tails.csv", tails)
        latency = [{"trial": t, "backend": b, "path": p, "bytes": s, "min_ns": 1, "median_ns": 2, "max_ns": 3}
                   for t,b,p,s in itertools.product(range(1,4), report.BACKENDS, report.PATHS, (16,64,256,4096,65536))]
        pressure = [{"trial": t, "backend": b, "path": p, "phase": phase, "rss": 100, "peak_rss": 100,
                     "requested_bytes": 0 if phase == "freed" else 64, "live_blocks": 0 if phase == "freed" else 1}
                    for t,b,p,phase in itertools.product(range(1,4), report.BACKENDS, report.PATHS,
                                                         ("dense", "sparse", "churn_dense", "churn_sparse", "freed"))]
        footprint = [{"trial": t, "backend": b, "statistics": s, "heap": h,
                      "baseline_rss": 100, "live_rss": 200, "freed_rss": 100, "collected_rss": 100, "peak_rss": 200}
                     for t in range(1,4) for b in report.BACKENDS for s in ("disabled", "basic")
                     for h in (("no",) if b == "standard" else ("no", "yes"))]
        heap = [{"trial": t, "backend": b, "operation": operation, "median_ns": 1, "operations": 2000}
                for t,b,operation in itertools.product(range(1,4), ("mimalloc", "jemalloc"), ("owns", "collect", "reset"))]
        for name, rows in (("latency", latency), ("pressure", pressure), ("footprint", footprint), ("heap", heap)):
            measurement.write_rows(self.directory / f"{name}.csv", rows)
        full = [{"trial": t, "backend": b, "workload": w, "bytes": size, "alignment": alignment,
                 "threads": threads, "statistics": tracking, "heap": heap, "median_ns_per_operation": 1}
                for t in range(1,4) for b,w,size,alignment,threads,tracking,heap in report.sweep_scenarios()]
        measurement.write_rows(self.directory / "full.csv", full)
        (self.directory / "sweep-environment.json").write_text(json.dumps({"scenarios_per_trial": 1001}))

    def test_complete_matrix(self):
        self.assertEqual(report.validate(self.directory)["trials"], 3)

    def test_missing_scenario_rejected(self):
        rows = report.read(self.directory / "scaling.csv")
        measurement.write_rows(self.directory / "scaling.csv", rows[:-1])
        with self.assertRaisesRegex(ValueError, "Missing"):
            report.validate(self.directory)

    def test_duplicate_scenario_rejected(self):
        rows = report.read(self.directory / "scaling.csv")
        rows[-1] = rows[0]
        measurement.write_rows(self.directory / "scaling.csv", rows)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            report.validate(self.directory)

    def test_nonfinite_measurement_rejected(self):
        rows = report.read(self.directory / "scaling.csv")
        rows[0]["operations_per_second"] = "nan"
        measurement.write_rows(self.directory / "scaling.csv", rows)
        with self.assertRaisesRegex(ValueError, "Invalid"):
            report.validate(self.directory)

    def test_nonfinite_memory_rejected(self):
        rows = report.read(self.directory / "scaling.csv")
        rows[0]["peak_rss"] = "nan"
        measurement.write_rows(self.directory / "scaling.csv", rows)
        with self.assertRaisesRegex(ValueError, "numeric"):
            report.validate(self.directory)

    def test_percentiles_are_individual_samples(self):
        self.assertEqual(measurement.percentile(list(range(1, 101)), .99), 99)
        with self.assertRaises(ValueError):
            measurement.percentile([], .99)

    def test_missing_memory_phase_rejected(self):
        rows = report.read(self.directory / "pressure.csv")
        measurement.write_rows(self.directory / "pressure.csv", rows[:-1])
        with self.assertRaisesRegex(ValueError, "pressure scenarios"):
            report.validate(self.directory)

    def test_outstanding_requests_rejected(self):
        rows = report.read(self.directory / "pressure.csv")
        next(row for row in rows if row["phase"] == "freed")["live_blocks"] = "1"
        measurement.write_rows(self.directory / "pressure.csv", rows)
        with self.assertRaisesRegex(ValueError, "outstanding"):
            report.validate(self.directory)

    def test_nonempty_wrong_schema_rejected(self):
        (self.directory / "latency.csv").write_text("value\n1\n")
        with self.assertRaisesRegex(ValueError, "schema"):
            report.validate(self.directory)

    def test_missing_full_sweep_workload_rejected(self):
        rows = report.read(self.directory / "full.csv")
        measurement.write_rows(self.directory / "full.csv", rows[:-1])
        with self.assertRaisesRegex(ValueError, "full sweep"):
            report.validate(self.directory)

    def test_same_missing_scenario_all_trials_cannot_self_certify(self):
        rows = report.read(self.directory / "full.csv")
        rows = [row for row in rows if row["workload"] != "stack_mark_rewind"]
        measurement.write_rows(self.directory / "full.csv", rows)
        (self.directory / "sweep-environment.json").write_text(json.dumps({"scenarios_per_trial": 989}))
        with self.assertRaisesRegex(ValueError, "full sweep"):
            report.validate(self.directory)

    def test_chart_xml_and_escaping(self):
        document = report.chart("A < B & C", "units", [1,2], [("standard", "native", [1,2])], "environment")
        tree = ET.fromstring(document)
        self.assertEqual(tree.find("{http://www.w3.org/2000/svg}title").text, "A < B & C")

    def test_generated_reports_follow_repository_text_contract(self):
        manifest = json.loads((self.directory / "environment.json").read_text())
        manifest.update(logical_cpus=4, run_url="https://github.com/example/project/actions/runs/1",
                        performance_gate="Report only", tail_method="Individual samples")
        (self.directory / "environment.json").write_text(json.dumps(manifest))
        output = self.directory / "charts"
        isolated_root = self.directory / "repo"
        for prior in (None, {"logical_cpus": 1}):
            with self.subTest(baseline=prior):
                if prior:
                    previous = isolated_root / "docs/results/current/test"
                    previous.mkdir(parents=True)
                    (previous / "environment.json").write_text(json.dumps(prior))
                with mock.patch("sys.argv", ["report", str(self.directory), str(output), "--label", "test"]), \
                     mock.patch.object(report, "__file__", str(isolated_root / "tools/ci/performance-report.py")):
                    report.main()
                for path in output.iterdir():
                    content = path.read_bytes()
                    self.assertTrue(content.endswith(b"\n"), path.name)
                    self.assertFalse(content.endswith(b"\n\n"), path.name)
                    self.assertNotIn(b"\r", content, path.name)
                self.assertEqual(len(list(output.glob("*.svg"))), 12)
                for path in output.glob("*.svg"):
                    ET.parse(path)
                for name in report.CHARTS:
                    english = ET.parse(output / f"{name}.svg").getroot()
                    chinese = ET.parse(output / f"{name}.zh-CN.svg").getroot()
                    self.assertEqual(chinese.get("{http://www.w3.org/XML/1998/namespace}lang"), "zh-CN")
                    shapes = {"path", "polyline", "circle", "rect"}
                    geometry = lambda tree: [(node.tag, node.attrib) for node in tree.iter()
                                             if node.tag.rsplit("}", 1)[-1] in shapes]
                    self.assertEqual(geometry(english), geometry(chinese), name)

    def test_chinese_chart_has_localized_title_axes_and_legend(self):
        title = "Cross-thread throughput"
        content = report.chart(title, "Million allocation/free pairs per second · higher is better",
                               [1, 2], [("standard", "native", [1, 2])], "source", xlabel="Threads", language="zh-CN")
        tree = ET.fromstring(content)
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        self.assertEqual(tree.find("svg:title", namespace).text, report.localized(title, "zh-CN"))
        text = " ".join(node.text or "" for node in tree.findall(".//svg:text", namespace))
        self.assertIn(report.localized("Threads", "zh-CN"), text)
        self.assertIn(report.localized("Native", "zh-CN"), text)
        self.assertNotIn("higher is better", text)

    def test_charts_only_preserves_existing_analysis_and_summary(self):
        manifest = json.loads((self.directory / "environment.json").read_text())
        manifest["logical_cpus"] = 4
        (self.directory / "environment.json").write_text(json.dumps(manifest))
        output = self.directory / "charts"
        output.mkdir()
        existing = {"README.md": b"Existing baseline signals.\n", "summary.json": b'{"source":"original"}\n'}
        for name, content in existing.items():
            (output / name).write_bytes(content)
        with mock.patch("sys.argv", ["report", str(self.directory), str(output), "--label", "test", "--charts-only"]):
            report.main()
        for name, content in existing.items():
            self.assertEqual((output / name).read_bytes(), content)
        self.assertEqual(len(list(output.glob("*.svg"))), 12)

    def test_report_pages_embed_all_platform_dimensions(self):
        for language, suffix in (("en", ""), ("zh-CN", ".zh-CN")):
            path = ROOT / "docs" / f"performance{suffix}.md"
            content = path.read_text(encoding="utf-8")
            for platform, chart in itertools.product(("linux-x64", "windows-x64", "macos-arm64"), report.CHARTS):
                self.assertRegex(content, r'!\[[^\]]+\]\(images/performance/' + platform + '/' + chart + suffix + r'\.svg\)')
        for name in ("README.md", "README.zh-CN.md"):
            self.assertIn("docs/images/workload-comparison.png", (ROOT / name).read_text(encoding="utf-8"))

    def test_statistics_columns_have_distinct_styles_and_matching_legend(self):
        document = report.chart("Statistics", "ns", [64],
                                [("mimalloc", path, [1]) for path in report.PATHS], "environment", bars=True)
        tree = ET.fromstring(document)
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        columns = [node for node in tree.findall(".//svg:rect", namespace)
                   if node.get("data-path")]
        styles = {(node.get("fill"), node.get("opacity")) for node in columns}
        self.assertEqual(len(styles), 3)
        self.assertEqual(len(columns), 6)  # Three columns and three matching legend swatches.
        self.assertEqual({node.get("data-path") for node in columns}, set(report.PATHS))
        for path in report.PATHS:
            matching = [node for node in columns if node.get("data-path") == path]
            self.assertEqual([(node.get("fill"), node.get("opacity")) for node in matching],
                             [(matching[0].get("fill"), matching[0].get("opacity"))]*2)


if __name__ == "__main__":
    unittest.main()
