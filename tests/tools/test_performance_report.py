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


if __name__ == "__main__":
    unittest.main()
