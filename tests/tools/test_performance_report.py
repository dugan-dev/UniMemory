import importlib.util
import itertools
import json
from pathlib import Path
import shutil
import statistics
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
        pressure = [{"trial": t, "backend": b, "path": p, "phase": phase, "baseline_rss": 10, "rss": 100, "peak_rss": 100,
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


class CombinedReportTests(unittest.TestCase):
    source = ROOT / "docs/results/current/linux-x64"

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def copy_profiles(self):
        destination = self.directory / "linux-x64"
        for backend in report.BACKENDS:
            shutil.copytree(self.source / backend, destination / backend)
        return destination

    def test_missing_profile_rejected_before_writing_any_charts(self):
        current = self.directory / "current"
        for platform in report.PLATFORM_LABELS:
            for backend in report.BACKENDS:
                shutil.copytree(ROOT / "docs/results/current" / platform / backend, current / platform / backend)
        (current / "macos-arm64/jemalloc/ON/environment.json").unlink()
        output = self.directory / "charts"
        with self.assertRaisesRegex(ValueError, "Missing performance profile"):
            report.refresh_reports(current, output)
        self.assertFalse(output.exists())

    def test_stale_source_rejected(self):
        with self.assertRaisesRegex(ValueError, "Stale source revision"):
            report.load_combined(self.source, expected_revision="0" * 40)

    def test_complete_refresh_preserves_profile_analysis_and_refreshes_every_chart(self):
        output = self.directory / "charts"
        preserved = []
        for platform, backend, mode in itertools.product(report.PLATFORM_LABELS, report.BACKENDS, ("OFF", "ON")):
            directory = output / platform / backend / mode
            directory.mkdir(parents=True)
            for name, content in (("README.md", "Recorded baseline signals.\n"), ("summary.json", '{"baseline":"recorded"}\n')):
                path = directory / name
                path.write_text(content)
                preserved.append((path, path.read_bytes()))
        result = report.refresh_reports(ROOT / "docs/results/current", output)
        self.assertEqual(result["profiles"], 18)
        self.assertEqual(result["charts"], 252)
        charts = list(output.rglob("*.svg"))
        self.assertEqual(len(charts), 252)
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        for path in charts:
            document = ET.parse(path).getroot()
            metadata = json.loads(document.find("svg:metadata", namespace).text)
            self.assertEqual(metadata["source_revision"], result["source_revision"], path)
            content = path.read_bytes()
            self.assertTrue(content.endswith(b"\n"), path)
            self.assertNotIn(b"\r", content, path)
            self.assertEqual(content, (ROOT / "docs/images/performance" / path.relative_to(output)).read_bytes(), path)
        for path, content in preserved:
            self.assertEqual(path.read_bytes(), content)

    def test_different_profile_revisions_or_protocols_rejected(self):
        for key in ("source_revision", "measurement_protocol", "pressure_protocol"):
            with self.subTest(key=key):
                current = self.copy_profiles()
                path = current / "jemalloc/ON/environment.json"
                manifest = json.loads(path.read_text())
                manifest[key] = "different"
                path.write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError, "different source revisions, protocols"):
                    report.load_combined(current)
                shutil.rmtree(current)

    def test_aggregate_svg_selects_off_baselines_and_on_statistics_with_separate_provenance(self):
        data = report.load_combined(self.source)
        output = self.directory / "charts"
        output.mkdir()
        for language in report.LANGUAGES:
            report.render_combined(data, output, "Linux x64", language)
        self.assertEqual(len(list(output.glob("*.svg"))), 12)
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        document = ET.parse(output / "statistics.svg").getroot()
        metadata = json.loads(document.find("svg:metadata", namespace).text)
        self.assertEqual(set(metadata["profiles"]), {f"{backend}/{mode}" for backend in report.BACKENDS for mode in ("OFF", "ON")})
        self.assertNotIn("executable_sha256", metadata)
        for entry in metadata["series"]:
            backend, path = entry["backend"], entry["path"]
            mode = "ON" if path == "basic" else "OFF"
            self.assertEqual(entry["profile"], f"{backend}/{mode}")
            measured = report.read(self.source / backend / mode / "tails.csv")
            expected = [statistics.median(float(row["allocate_p99_ns"]) for row in measured
                                          if row["path"] == path and int(row["bytes"]) == size)
                        for size in (16, 64, 256, 4096, 65536)]
            self.assertEqual(entry["values"], expected)
            self.assertEqual(metadata["profiles"][entry["profile"]]["manifest"],
                             json.loads((self.source / backend / mode / "environment.json").read_text()))
            self.assertEqual(metadata["profiles"][entry["profile"]]["sweep_manifest"],
                             json.loads((self.source / backend / mode / "sweep-environment.json").read_text()))
        text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
        self.assertIn("independent builds/runners", text)
        self.assertIn("no causal cost", text)
        for name in report.CHARTS:
            english = ET.parse(output / f"{name}.svg").getroot()
            chinese = ET.parse(output / f"{name}.zh-CN.svg").getroot()
            self.assertEqual(json.loads(english.find("svg:metadata", namespace).text),
                             json.loads(chinese.find("svg:metadata", namespace).text))

    def test_normalized_workloads_use_current_off_api_measurements(self):
        data = report.load_combined(self.source)
        report.render_combined(data, self.directory, "Linux x64", "en")
        document = ET.parse(self.directory / "workloads.svg").getroot()
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        metadata = json.loads(document.find("svg:metadata", namespace).text)
        self.assertEqual(metadata["data_file"], "full.csv")
        medians = {}
        for backend in report.BACKENDS:
            rows = report.read(self.source / backend / "OFF/full.csv")
            medians[backend] = [statistics.median(float(row["median_ns_per_operation"]) for row in rows
                                                 if (row["workload"], row["bytes"], row["alignment"]) == scenario
                                                 and row["statistics"] == "disabled" and row["heap"] == "no" and row["threads"] == "1")
                                for scenario in report.API_SCENARIOS]
        for entry in metadata["series"]:
            self.assertEqual(entry["values"], [value / baseline for value, baseline in
                                              zip(medians[entry["backend"]], medians["standard"])])
            self.assertEqual(entry["profile"], f"{entry['backend']}/OFF")
        text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
        self.assertIn("independent OFF builds/runners", text)

    def test_retention_subtracts_each_trial_baseline_before_median(self):
        data = report.load_combined(self.source)
        baselines = {"1": 10, "2": 100, "3": 30}
        resident = {"1": 100, "2": 200, "3": 110}
        for row in data["tables"]["pressure"]:
            row["baseline_rss"] = baselines[row["trial"]] * 1024**2
            row["rss"] = resident[row["trial"]] * 1024**2
        report.render_combined(data, self.directory, "Linux x64", "en")
        document = ET.parse(self.directory / "retention.svg").getroot()
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        metadata = json.loads(document.find("svg:metadata", namespace).text)
        self.assertEqual(metadata["metric"], "rss_minus_baseline_rss")
        self.assertEqual(metadata["unit"], "MiB")
        for entry in metadata["series"]:
            self.assertEqual(entry["values"], [90] * 5)
        text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
        self.assertIn("Resident MiB above baseline", text)

    def test_checked_in_aggregate_charts_use_current_profile_sources_and_public_platform_labels(self):
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        for platform, label in report.PLATFORM_LABELS.items():
            source = ROOT / "docs/results/current" / platform
            expected_revision = json.loads((source / "standard/OFF/environment.json").read_text())["source_revision"]
            summary = json.loads((ROOT / "docs/images/performance" / platform / "summary.json").read_text())
            self.assertEqual(summary["source_revision"], expected_revision)
            analysis = (ROOT / "docs/images/performance" / platform / "README.md").read_text()
            self.assertIn(expected_revision, analysis)
            self.assertNotIn("Baseline signals", analysis)
            for name, language in itertools.product(report.CHARTS, report.LANGUAGES):
                suffix = "" if language == "en" else ".zh-CN"
                path = ROOT / "docs/images/performance" / platform / f"{name}{suffix}.svg"
                document = ET.parse(path).getroot()
                metadata = json.loads(document.find("svg:metadata", namespace).text)
                self.assertEqual(metadata["source_revision"], expected_revision, path)
                for key, profile in metadata["profiles"].items():
                    self.assertEqual(profile["manifest"], json.loads((source / key / "environment.json").read_text()), path)
                text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
                self.assertIn(label, text)
                self.assertNotIn("arm64", text.lower())


if __name__ == "__main__":
    unittest.main()
