import importlib.util
import itertools
import json
from pathlib import Path
import tempfile
import unittest
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
                    "bytes": size, "threads": threads, "operations_per_second": 1}
                   for trial, backend, path, workload, size, threads in itertools.product(
                       range(1, 4), report.BACKENDS, report.PATHS, ("same_thread", "handoff"), (64, 4096, 65536), report.THREADS)]
        tails = [{"trial": trial, "backend": backend, "path": path, "bytes": size,
                  "samples": 8192, "allocate_p99_ns": 1}
                 for trial, backend, path, size in itertools.product(range(1, 4), report.BACKENDS, report.PATHS, (16,64,256,4096,65536))]
        measurement.write_rows(self.directory / "scaling.csv", scaling)
        measurement.write_rows(self.directory / "tails.csv", tails)
        for file in ("latency.csv", "pressure.csv", "footprint.csv", "heap.csv"):
            (self.directory / file).write_text("value\n1\n")

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

    def test_percentiles_are_individual_samples(self):
        self.assertEqual(measurement.percentile(list(range(1, 101)), .99), 99)
        with self.assertRaises(ValueError):
            measurement.percentile([], .99)

    def test_chart_xml_and_escaping(self):
        document = report.chart("A < B & C", "units", [1,2], [("standard", "native", [1,2])], "environment")
        tree = ET.fromstring(document)
        self.assertEqual(tree.find("{http://www.w3.org/2000/svg}title").text, "A < B & C")


if __name__ == "__main__":
    unittest.main()
