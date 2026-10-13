import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("recovery", Path(__file__).resolve().parents[2] / "tools/ci/correctness-recovery.py")
recovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recovery)


class RecoveryEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.run = {"id": 123, "head_sha": "a" * 40, "html_url": "https://github.com/example/repo/actions/runs/123",
                    "head_repository": {"full_name": "example/repo"}, "path": ".github/workflows/performance.yml",
                    "event": "workflow_dispatch", "status": "completed"}
        self.jobs = [{"id": 1, "name": "passed baseline job", "status": "completed", "conclusion": "success"},
                     {"id": 2, "name": "failed job", "status": "completed", "conclusion": "failure"}]

    def test_safe_changes_preserve_original_source_of_passed_jobs(self):
        result = recovery.baseline_evidence(self.run, self.jobs, "example/repo", "a" * 7,
                                           ["tools/ci/analyze.py", "tests/tools/test_publication.py", "README.md"])
        self.assertFalse(result["requires_regression"])
        self.assertEqual([job["id"] for job in result["passed_baseline_jobs"]], [1])
        self.assertEqual(result["passed_baseline_jobs"][0]["source_revision"], "a" * 40)

    def test_library_cmake_and_cpp_test_changes_require_regression(self):
        for path in ("include/unimem/memory.h", "CMakeLists.txt", "tests/test_memory.cpp", "vcpkg.json"):
            with self.subTest(path=path):
                result = recovery.baseline_evidence(self.run, self.jobs, "example/repo", "a" * 40, [path])
                self.assertTrue(result["requires_regression"])
                self.assertFalse(result["baseline_reuse_eligible"])

    def test_untrusted_or_incomplete_baseline_rejected(self):
        for key, value in (("path", ".github/workflows/other.yml"), ("event", "pull_request"),
                           ("status", "in_progress"), ("head_sha", "b" * 40),
                           ("head_repository", {"full_name": "fork/repo"})):
            with self.subTest(key=key):
                with self.assertRaises(RuntimeError):
                    recovery.baseline_evidence(dict(self.run, **{key: value}), self.jobs, "example/repo", "a" * 7, [])

    def test_failed_and_incomplete_jobs_cannot_be_inherited(self):
        with self.assertRaises(RuntimeError):
            recovery.baseline_evidence(self.run, [self.jobs[1]], "example/repo", "a" * 7, [])

    def test_explicit_fixture_changes_require_targeted_tests_only(self):
        fixtures = ["tests/allocation_failure.cpp", "tests/headers/factory_statistics.cpp",
                    "tests/headers/inheritance.cpp"]
        result = recovery.baseline_evidence(self.run, self.jobs, "example/repo", "a" * 7, fixtures)
        self.assertFalse(result["requires_regression"])
        self.assertEqual(result["targeted_fixtures"], fixtures)

    def test_performance_and_aggregate_checks_are_not_correctness_evidence(self):
        for name in ("performance acceptance", "measure-linux / measure", "diagnosis-macos / diagnose",
                     "windows / build / build acceptance", "linux / correctness", "overview"):
            self.assertFalse(recovery.correctness_job(name), name)

    def test_audited_baseline_requires_actual_dispatch_event(self):
        run = dict(self.run, id=38262808305)
        recovery.baseline_evidence(run, self.jobs, "example/repo", "a" * 7, [])
        with self.assertRaises(RuntimeError):
            recovery.baseline_evidence(dict(run, event="push"), self.jobs, "example/repo", "a" * 7, [])


if __name__ == "__main__":
    unittest.main()
