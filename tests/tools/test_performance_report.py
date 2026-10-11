import importlib.util
import gzip
import hashlib
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
        manifest = json.loads((self.directory / "environment.json").read_text(encoding="utf-8"))
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
        manifest = json.loads((self.directory / "environment.json").read_text(encoding="utf-8"))
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
        self.assertEqual(result["charts"], 36)
        charts = list(output.rglob("*.svg"))
        self.assertEqual(len(charts), 36)
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        for path in charts:
            document = ET.parse(path).getroot()
            metadata = json.loads(document.find("svg:metadata", namespace).text)
            self.assertEqual(metadata["source_revision"], result["source_revision"], path)
            content = path.read_bytes()
            self.assertTrue(content.endswith(b"\n"), path)
            self.assertNotIn(b"\r", content, path)
            self.assertNotIn("manifest", json.dumps(metadata), path)
            self.assertLess(len(content), 25000, path)
            for profile in metadata["profiles"].values():
                self.assertTrue((ROOT / profile["source"]).is_file(), path)
        for path, content in preserved:
            self.assertEqual(path.read_bytes(), content)

    def test_different_profile_revisions_or_protocols_rejected(self):
        for key in ("source_revision", "measurement_protocol", "pressure_protocol"):
            with self.subTest(key=key):
                current = self.copy_profiles()
                try:
                    path = current / "jemalloc/ON/environment.json"
                    manifest = json.loads(path.read_text(encoding="utf-8"))
                    manifest[key] = "different"
                    path.write_text(json.dumps(manifest), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, "source|protocol"):
                        report.load_combined(current)
                finally:
                    self.assertTrue(current.resolve().is_relative_to(self.directory.resolve()))
                    shutil.rmtree(current)

    def test_controlled_profiles_reject_different_host_or_sdk_hashes(self):
        suite = {"host_id": "synthetic-shared-host"}

        def manifest(directory):
            value = json.loads((directory / "environment.json").read_text(encoding="utf-8"))
            value.update(schema=3, trials=10, controlled_suite=dict(suite),
                         measurement_protocol="native-api-calibrated-v3", cpu_model="synthetic CPU", logical_cpus=4)
            for binary in value["build_profile"]["sdk_binaries"].values():
                binary["sha256"] = "0" * 64
            return value

        with mock.patch.object(report, "validate", side_effect=manifest), mock.patch.object(report, "resolve_path", side_effect=lambda path: path):
            combined = report.load_combined(self.source)
            self.assertEqual(combined["provenance"]["measurement_schema"], 3)

        def different_host(directory):
            value = manifest(directory)
            if directory.parts[-2:] == ("jemalloc", "ON"):
                value["controlled_suite"]["host_id"] = "other-host"
            return value

        with mock.patch.object(report, "validate", side_effect=different_host), mock.patch.object(report, "resolve_path", side_effect=lambda path: path):
            with self.assertRaisesRegex(ValueError, "one controlled host"):
                report.load_combined(self.source)

        def different_sdk(directory):
            value = manifest(directory)
            if directory.parts[-2:] == ("mimalloc", "ON"):
                for binary in value["build_profile"]["sdk_binaries"].values():
                    binary["sha256"] = "f" * 64
            return value

        with mock.patch.object(report, "validate", side_effect=different_sdk), mock.patch.object(report, "resolve_path", side_effect=lambda path: path):
            with self.assertRaisesRegex(ValueError, "different SDK binaries"):
                report.load_combined(self.source)

        def different_flags(directory):
            value = manifest(directory)
            if directory.parts[-2:] == ("standard", "ON"):
                value["build_profile"]["build_parameters"]["release_flags"] += " changed"
            return value

        with mock.patch.object(report, "validate", side_effect=different_flags), mock.patch.object(report, "resolve_path", side_effect=lambda path: path):
            with self.assertRaisesRegex(ValueError, "optimization flags"):
                report.load_combined(self.source)

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
            reducer = statistics.mean if data["provenance"]["measurement_schema"] == 3 else statistics.median
            expected = [reducer(float(row["allocate_p99_ns"]) for row in measured
                                          if row["path"] == path and int(row["bytes"]) == size)
                        for size in (16, 64, 256, 4096, 65536)]
            self.assertEqual(entry["values"], expected)
            profile = metadata["profiles"][entry["profile"]]
            self.assertEqual(profile["source"], f"docs/results/current/linux-x64/{backend}/{mode}/environment.json")
            self.assertEqual(profile["sweep_source"], f"docs/results/current/linux-x64/{backend}/{mode}/sweep-environment.json")
            self.assertNotIn("manifest", profile)
        text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
        if data["provenance"]["measurement_schema"] == 3:
            self.assertIn("one controlled host", text)
            self.assertIn("includes statistics feature cost", text)
        else:
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
        repeated = data["provenance"]["measurement_schema"] == 3
        reducer = statistics.mean if repeated else statistics.median
        metric = "mean_ns_per_operation" if repeated else "median_ns_per_operation"
        for backend in report.BACKENDS:
            rows = report.read(self.source / backend / "OFF/full.csv")
            medians[backend] = [reducer(float(row[metric]) for row in rows
                                                 if (row["workload"], row["bytes"], row["alignment"]) == scenario
                                                 and row["statistics"] == "disabled" and row["heap"] == "no" and row["threads"] == "1")
                                for scenario in report.API_SCENARIOS]
        for entry in metadata["series"]:
            self.assertEqual(entry["values"], [value / baseline for value, baseline in
                                              zip(medians[entry["backend"]], medians["standard"])])
            self.assertEqual(entry["profile"], f"{entry['backend']}/OFF")
        text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
        self.assertIn("one controlled host" if repeated else "independent OFF builds/runners", text)

    def test_retention_subtracts_each_trial_baseline_before_median(self):
        data = report.load_combined(self.source)
        baselines = {"1": 10, "2": 100, "3": 30}
        resident = {"1": 100, "2": 200, "3": 110}
        for row in data["tables"]["pressure"]:
            trial = str((int(row["trial"]) - 1) % 3 + 1)
            row["baseline_rss"] = baselines[trial] * 1024**2
            row["rss"] = resident[trial] * 1024**2
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
            expected_revision = json.loads((source / "standard/OFF/environment.json").read_text(encoding="utf-8"))["source_revision"]
            summary = json.loads((ROOT / "docs/images/performance" / platform / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["source_revision"], expected_revision)
            for name, language in itertools.product(report.CHARTS, report.LANGUAGES):
                suffix = "" if language == "en" else ".zh-CN"
                path = ROOT / "docs/images/performance" / platform / f"{name}{suffix}.svg"
                document = ET.parse(path).getroot()
                metadata = json.loads(document.find("svg:metadata", namespace).text)
                self.assertEqual(metadata["source_revision"], expected_revision, path)
                for key, profile in metadata["profiles"].items():
                    self.assertEqual(profile["source"], f"docs/results/current/{platform}/{key}/environment.json", path)
                    self.assertEqual(json.loads((ROOT / profile["source"]).read_text(encoding="utf-8"))["source_revision"], expected_revision)
                    self.assertNotIn("manifest", profile)
                text = " ".join(node.text or "" for node in document.findall(".//svg:text", namespace))
                self.assertIn(label, text)
                self.assertNotIn("arm64", text.lower())


class RepeatedReportTests(unittest.TestCase):
    """Synthetic fixtures exercise v3 without relabeling retained legacy data."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name) / "profile"
        shutil.copytree(ROOT / "docs/results/current/linux-x64/standard/OFF", self.directory)
        manifest = json.loads((self.directory / "environment.json").read_text(encoding="utf-8"))
        manifest.update(schema=3, trials=10, measurement_protocol="native-api-calibrated-v3", executable_sha256="a" * 64)
        calibration = {"latency": {str(size): 100000000 for size in (16, 64, 256, 4096, 65536)},
                       "scaling": {f"{workload}:{size}:{threads}": 8 for workload, size, threads in itertools.product(("scaling", "handoff"), (64, 4096, 65536), report.THREADS)}}
        manifest["calibration"] = {**calibration, "heap": {}}
        suite = {"schema": 3, "protocol": manifest["measurement_protocol"], "trials": 10,
                 "host_id": "synthetic-test-host", "cpu_model": manifest["cpu_model"],
                 "affinity": {"method": "test fixture", "cpus": [0], "pinned": True},
                 "profiles": [{"backend": backend, "statistics": mode, "executable_sha256": "a" * 64,
                               "sweep_executable_sha256": "b" * 64, "sweep_source_sha256": "c" * 64,
                               "heap_operation_counts": {},
                               **{field: manifest["build_profile"]["build_parameters"].get(field) for field in report.BUILD_COMPARISON_FIELDS}}
                              for backend, mode in itertools.product(report.BACKENDS, ("OFF", "ON"))],
                 "schedule": [{"trial": trial, "backend": backend, "statistics": mode}
                              for trial, backend, mode in itertools.product(range(1, 11), report.BACKENDS, ("OFF", "ON"))],
                 "calibration": calibration}
        manifest["controlled_suite"] = suite
        sweep = json.loads((self.directory / "sweep-environment.json").read_text(encoding="utf-8"))
        sweep.update(schema=3, trials=10, repetitions=1, sequential_processes=True,
                     measurement_protocol=manifest["measurement_protocol"], executable_sha256="b" * 64,
                     benchmark_source_sha256="c" * 64,
                     **{key: manifest[key] for key in ("source_revision", "run_id", "run_url")})
        for name in report.benchmark_statistics.SCENARIO_FIELDS:
            if name == "native":
                continue
            first = [row for row in report.read(self.directory / f"{name}.csv") if row["trial"] == "1"]
            rows = []
            for trial in range(1, 11):
                for old in first:
                    row = {**old, "trial": str(trial)}
                    if name in ("latency", "full", "heap"):
                        suffix = "_per_operation" if name == "full" else ""
                        value = float(trial if name == "latency" and row.get("path") != "native" else 2)
                        for field in ("min", "mean", "median", "max"):
                            row[field + "_ns" + suffix] = str(value)
                        row["samples_ns" + suffix] = str(value)
                        if name == "latency":
                            row["operations"] = str(calibration["latency"][row["bytes"]])
                        row["seconds"] = str(value * int(row["operations"]) / 1e9)
                    if name == "scaling":
                        row["rounds"] = "8"
                        row["operations_per_second"] = str(int(row["operations"]) / float(row["seconds"]))
                    if name == "pressure":
                        row["baseline_rss"] = str(trial * 1024 ** 2)
                        row["rss"] = str((trial + trial ** 2) * 1024 ** 2)
                    if name == "tails":
                        for field in ("allocate_p50_ns", "allocate_p95_ns", "allocate_p99_ns", "allocate_p999_ns", "allocate_max_ns", "free_p99_ns", "clock_p50_ns"):
                            row[field] = "1"
                    rows.append(row)
            if rows:
                measurement.write_rows(self.directory / f"{name}.csv", rows)
        counts = {":".join(row[field] for field in ("workload", "bytes", "alignment", "threads", "heap")): int(row["operations"])
                  for row in report.read(self.directory / "full.csv")}
        operation_file = self.directory / "sweep-operations.txt"
        operation_file.write_text("".join(f"{key},{count}\n" for key, count in sorted(counts.items())), encoding="utf-8")
        digest = hashlib.sha256(operation_file.read_bytes()).hexdigest()
        sweep["operations_file_sha256"] = digest
        for profile in suite["profiles"]:
            profile["full_operations_sha256"] = digest
        manifest["trace_storage"] = {"location": "workflow artifacts", "retention_days": 90}
        trace_records = []
        manifest["tail_trace_evidence"] = {}
        for row in report.read(self.directory / "tails.csv"):
            name = f"raw/{row['trial']}-{row['backend']}-{row['path']}-{row['bytes']}.csv.gz"
            evidence = {"sha256": "e" * 64, "samples": 8192}
            manifest["tail_trace_evidence"][name] = evidence
            trace_records.append({"path": name, **evidence, "quantiles": {key: float(row[key]) for key in row if key.endswith("_ns")}})
        ledger = {"schema": 1, "source_revision": manifest["source_revision"], "run_id": str(manifest["run_id"]),
                  "method": "Nearest-rank allocation/free quantiles; clock median; all 8192 operation samples retained in workflow artifact",
                  "traces": trace_records}
        (self.directory / "tail-traces.json").write_text(json.dumps(ledger), encoding="utf-8")
        (self.directory / "environment.json").write_text(json.dumps(manifest), encoding="utf-8")
        (self.directory / "controlled-suite.json").write_text(json.dumps(suite), encoding="utf-8")
        (self.directory / "sweep-environment.json").write_text(json.dumps(sweep), encoding="utf-8")
        report.benchmark_statistics.write_summary(self.directory)

    def make_operation_traces(self):
        manifest = report.read_json(self.directory / "environment.json")
        payloads = {}
        for row in report.read(self.directory / "tails.csv"):
            key = row["backend"], row["path"], row["bytes"]
            if key not in payloads:
                header = "backend,path,bytes,sample,allocate_ns,free_ns,clock_ns\n"
                source = header + "".join(f"{row['backend']},{row['path']},{row['bytes']},{sample},1,1,1\n" for sample in range(8192))
                payloads[key] = gzip.compress(source.encode("utf-8"), mtime=0)
            name = f"raw/{row['trial']}-{row['backend']}-{row['path']}-{row['bytes']}.csv.gz"
            path = self.directory / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(payloads[key])
            manifest["tail_trace_evidence"][name]["sha256"] = hashlib.sha256(payloads[key]).hexdigest()
        (self.directory / "environment.json").write_text(json.dumps(manifest), encoding="utf-8")
        return manifest

    def make_sustained_calibration(self):
        from benchmark_calibration import new_evidence, calibrate_pair
        manifest = report.read_json(self.directory / "environment.json")
        suite = manifest["controlled_suite"]
        manifest["measurement_protocol"] = suite["protocol"] = report.SUSTAINED_PROTOCOL
        suite.pop("calibration", None)
        evidence = new_evidence(manifest["logical_cpus"])
        calibration = {"latency": {}, "scaling": {}, "heap": {}}
        for kind, cases in manifest["calibration"].items():
            for case, target in cases.items():
                def probe(path, count, repeat):
                    seconds = .35 * count / target
                    row = {"backend": "standard", "path": path, "seconds": str(seconds)}
                    if kind == "scaling":
                        work, size, threads = case.split(":")
                        operations = int(threads) * 512 * count
                        row.update(bytes=size, threads=threads, workload="same_thread" if work == "scaling" else "handoff",
                                   rounds=str(count), operations=str(operations), operations_per_second=str(operations / seconds))
                    else:
                        row.update(bytes=case, operations=str(count), mean_ns=str(seconds * 1e9 / count))
                    return row
                oversubscribed = kind == "scaling" and int(case.split(":")[-1]) > manifest["logical_cpus"]
                decision = calibrate_pair(kind, case, ("native", "disabled"), probe,
                                          initial=8 if kind == "scaling" else 20000,
                                          count_limit=(32 if oversubscribed else 131072) if kind == "scaling" else 100000000,
                                          evidence=evidence, oversubscribed=oversubscribed)
                calibration[kind][case] = decision["count"]
        manifest["calibration"] = calibration
        for row_name in ("latency", "scaling", "full"):
            rows = report.read(self.directory / (row_name + ".csv"))
            for row in rows:
                if row_name == "latency":
                    row["operations"] = str(calibration["latency"][row["bytes"]])
                    row["seconds"] = str(float(row["mean_ns"]) * int(row["operations"]) / 1e9)
                if row_name == "full":
                    row["operations"] = "320"
                    row["seconds"] = str(float(row["mean_ns_per_operation"]) * 320 / 1e9)
                row["short_interval"] = str(float(row["seconds"]) < .25).lower()
            measurement.write_rows(self.directory / (row_name + ".csv"), rows)
        full_rows = [row for row in report.read(self.directory / "full.csv") if row["trial"] == "1"]
        counts = {":".join(row[field] for field in ("workload", "bytes", "alignment", "threads", "heap")): 320 for row in full_rows}
        full = {"schema": 1, "protocol": report.SUSTAINED_PROTOCOL, "effective_cpus": manifest["logical_cpus"], "counts": counts, "records": []}
        for row in full_rows:
            oversubscribed = int(row["threads"]) > manifest["logical_cpus"]
            full["records"].append({**{key: row[key] for key in ("backend", "workload", "bytes", "alignment", "threads", "statistics", "heap")},
                                    "operations": "320", "pilot_operations": "256;256;256", "pilot_seconds": "0.35;0.35;0.35",
                                    "sustained_goal_met": "true", "capped": "false", "count_limit": "262144" if oversubscribed else "100000000",
                                    "oversubscribed": str(oversubscribed).lower(), "estimated_seconds": "0.4375"})
        operation_file = self.directory / "sweep-operations.txt"
        operation_file.write_text("".join(f"{key},{count}\n" for key, count in sorted(counts.items())), encoding="utf-8")
        sweep = report.read_json(self.directory / "sweep-environment.json")
        sweep.update(measurement_protocol=report.SUSTAINED_PROTOCOL, operations_file_sha256=hashlib.sha256(operation_file.read_bytes()).hexdigest(),
                     full_calibration_sha256=report.canonical_json_sha(full))
        for profile in suite["profiles"]:
            profile.update(delivery_calibration=calibration, calibration_sha256=report.canonical_json_sha(evidence),
                           full_calibration_sha256=report.canonical_json_sha(full), full_operations_sha256=sweep["operations_file_sha256"])
        for name, value in (("environment.json", manifest), ("controlled-suite.json", suite), ("sweep-environment.json", sweep),
                            ("calibration-pilots.json", evidence), ("full-calibration.json", full)):
            (self.directory / name).write_text(json.dumps(value), encoding="utf-8")
        report.benchmark_statistics.write_summary(self.directory)
        return manifest, evidence

    def test_sustained_per_profile_calibration_and_short_intervals_validate(self):
        self.make_sustained_calibration()
        self.assertEqual(report.validate(self.directory)["measurement_protocol"], report.SUSTAINED_PROTOCOL)

    def test_sustained_pilot_decisions_must_replay_all_observations(self):
        manifest, evidence = self.make_sustained_calibration()
        decision = next(item for item in evidence["decisions"] if item["kind"] == "scaling" and item["case"] == "scaling:64:8")
        decision.update(count=100, count_limit=131072, oversubscribed=False, estimated_seconds={"native": .35, "disabled": .35})
        manifest["calibration"]["scaling"]["scaling:64:8"] = 100
        selected = {"calibration_sha256": report.canonical_json_sha(evidence)}
        (self.directory / "calibration-pilots.json").write_text(json.dumps(evidence), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "oversubscription|safety bound"):
            report.validate_calibration_pilots(self.directory, manifest, selected)

    def test_sustained_pilot_selection_rejects_a_false_duration_budget(self):
        manifest, evidence = self.make_sustained_calibration()
        for record in evidence["records"]:
            if record["kind"] == "scaling" and record["case"] == "scaling:64:8":
                record["row"]["seconds"] = "1"
                record["row"]["operations_per_second"] = record["row"]["operations"]
        decision = next(item for item in evidence["decisions"] if item["kind"] == "scaling" and item["case"] == "scaling:64:8")
        decision.update(count=32, estimated_seconds={"native": .35, "disabled": .35})
        manifest["calibration"]["scaling"]["scaling:64:8"] = 32
        selected = {"calibration_sha256": report.canonical_json_sha(evidence)}
        (self.directory / "calibration-pilots.json").write_text(json.dumps(evidence), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "all retained pilots"):
            report.validate_calibration_pilots(self.directory, manifest, selected)

    def test_ten_process_matrix_and_all_scenario_statistics_validate(self):
        self.assertEqual(report.validate(self.directory)["trials"], 10)
        summary = report.validate_summary(self.directory)
        entry = next(item for item in summary["tables"]["latency"] if item["scenario"] == {"backend": "standard", "path": "disabled", "bytes": "16"})
        self.assertEqual((entry["n"], entry["min"], entry["max"], entry["mean"]), (10, 1, 10, 5.5))
        self.assertAlmostEqual(entry["sample_sd"], statistics.stdev(range(1, 11)))
        self.assertAlmostEqual(entry["cv"], statistics.stdev(range(1, 11)) / 5.5)
        self.assertLess(entry["ci95_low"], entry["mean"])
        self.assertGreater(entry["ci95_high"], entry["mean"])
        pair = next(item for item in summary["paired"] if item["table"] == "latency" and item["scenario"] == {"backend": "standard", "bytes": "16"})
        self.assertAlmostEqual(pair["mean"], statistics.mean(2 / trial for trial in range(1, 11)))
        self.assertNotAlmostEqual(pair["mean"], 2 / 5.5)

    def test_losslessly_compressed_raw_tables_and_summary_validate(self):
        for path in [*self.directory.glob("*.csv"), self.directory / "summary.json"]:
            path.with_name(path.name + ".gz").write_bytes(gzip.compress(path.read_bytes(), mtime=0))
            path.unlink()
        self.assertEqual(report.validate(self.directory)["trials"], 10)

    def test_missing_duplicate_and_false_mean_rejected(self):
        path = self.directory / "latency.csv"
        original = report.read(path)
        for changed in (original[:-1], original[:-1] + [original[0]]):
            measurement.write_rows(path, changed)
            with self.assertRaisesRegex(ValueError, "Missing, duplicate"):
                report.validate(self.directory)
        original[0]["mean_ns"] = "999"
        measurement.write_rows(path, original)
        with self.assertRaisesRegex(ValueError, "calibrated|disagree"):
            report.validate(self.directory)

    def test_false_summary_and_missing_statistics_rejected(self):
        path = self.directory / "summary.json"
        summary = json.loads(path.read_text(encoding="utf-8"))
        summary["tables"]["latency"][0]["mean"] += 1
        path.write_text(json.dumps(summary))
        with self.assertRaisesRegex(ValueError, "disagrees"):
            report.validate_summary(self.directory)
        summary["tables"]["latency"].pop()
        path.write_text(json.dumps(summary), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Missing or duplicate"):
            report.validate_summary(self.directory)

    def test_false_bootstrap_interval_inside_sample_range_is_rejected(self):
        path = self.directory / "summary.json"
        summary = report.read_json(path)
        entry = next(item for item in summary["tables"]["latency"] if item["scenario"]["path"] == "disabled")
        entry["ci95_low"] = entry["ci95_high"] = entry["min"]
        path.write_text(json.dumps(summary), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "disagrees"):
            report.validate_summary(self.directory)

    def test_full_sweep_provenance_and_calibration_hash_rejected(self):
        path = self.directory / "sweep-environment.json"
        original = report.read_json(path)
        for key in ("source_revision", "measurement_protocol", "executable_sha256", "benchmark_source_sha256", "operations_file_sha256"):
            value = {**original, key: "different"}
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "provenance|calibration hash"):
                report.validate(self.directory)
        path.write_text(json.dumps(original), encoding="utf-8")
        with (self.directory / "sweep-operations.txt").open("a", encoding="utf-8") as stream:
            stream.write("unexpected,1\n")
        with self.assertRaisesRegex(ValueError, "calibration hash"):
            report.validate(self.directory)

    def test_delivery_counts_and_elapsed_arithmetic_are_enforced(self):
        latency_path = self.directory / "latency.csv"
        latency = report.read(latency_path)
        for changed in ({"operations": "1", "seconds": str(float(latency[0]["mean_ns"]) / 1e9)}, {"seconds": "900"}):
            rows = [{**latency[0], **changed}, *latency[1:]]
            measurement.write_rows(latency_path, rows)
            with self.assertRaisesRegex(ValueError, "count|interval disagree"):
                report.validate(self.directory)
        measurement.write_rows(latency_path, latency)
        scaling_path = self.directory / "scaling.csv"
        scaling = report.read(scaling_path)
        for changed in ({"seconds": "900"}, {"rounds": "1", "operations": str(int(scaling[0]["threads"]) * 512)}):
            rows = [{**scaling[0], **changed}, *scaling[1:]]
            measurement.write_rows(scaling_path, rows)
            with self.assertRaisesRegex(ValueError, "Scaling|scaling"):
                report.validate(self.directory)

    def test_heap_interval_counts_match_selected_profile_calibration(self):
        manifest = {"calibration": {"heap": {"collect": 250000000}}}
        row = {"operation": "collect", "operations": "250000000", "min_ns": "1", "mean_ns": "1",
               "median_ns": "1", "max_ns": "1", "samples_ns": "1", "seconds": "0.25"}
        report.validate_interval_rows([row], "heap", manifest)
        with self.assertRaisesRegex(ValueError, "operation count"):
            report.validate_interval_rows([{**row, "operations": "1", "seconds": "0.000000001"}], "heap", manifest)

    def test_strict_artifact_rejects_missing_corrupt_or_false_operation_trace(self):
        manifest = self.make_operation_traces()
        ledger = report.validate_tail_traces(self.directory, manifest, strict_artifact=True)
        self.assertEqual(len(ledger["traces"]), 100)
        name = sorted(manifest["tail_trace_evidence"])[0]
        path = self.directory / name
        original = path.read_bytes()
        path.unlink()
        with self.assertRaisesRegex(ValueError, "Missing or unexpected"):
            report.validate_tail_traces(self.directory, manifest, strict_artifact=True)
        path.write_bytes(gzip.compress(b"corrupt\n", mtime=0))
        with self.assertRaisesRegex(ValueError, "hash differs"):
            report.validate_tail_traces(self.directory, manifest, strict_artifact=True)
        content = gzip.decompress(original).decode("utf-8").replace(",1,1,1\n", ",2,1,1\n")
        changed = gzip.compress(content.encode("utf-8"), mtime=0)
        path.write_bytes(changed)
        manifest["tail_trace_evidence"][name]["sha256"] = hashlib.sha256(changed).hexdigest()
        with self.assertRaisesRegex(ValueError, "quantiles disagree"):
            report.validate_tail_traces(self.directory, manifest, strict_artifact=True)

    def test_outlier_is_retained_as_independent_observation(self):
        values = [1] * 9 + [100]
        summary = report.benchmark_statistics.numeric_summary(values)
        self.assertEqual((summary["n"], summary["min"], summary["max"], summary["mean"], summary["median"]), (10, 1, 100, 10.9, 1))
        self.assertAlmostEqual(summary["sample_sd"], statistics.stdev(values))

    def test_retention_mean_is_of_per_process_deltas(self):
        rows = report.read(self.directory / "pressure.csv")
        rows = [{**row, "delta": float(row["rss"]) - float(row["baseline_rss"])} for row in rows]
        values = report.aggregate(rows, lambda row: (row["backend"], row["path"], row["phase"]), "delta", mean=True)
        self.assertEqual(values["standard", "disabled", "dense"] / 1024 ** 2, statistics.mean(trial ** 2 for trial in range(1, 11)))


if __name__ == "__main__":
    unittest.main()
