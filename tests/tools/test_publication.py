import copy
import gzip
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("publisher", Path(__file__).resolve().parents[2] / "tools/ci/publish-performance.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.repository = "example/project"
        self.head, self.base = "a"*40, "b"*40
        self.branch = "automation/performance-dev"
        self.pr = {"state": "open", "changed_files": 2, "user": {"login": "github-actions[bot]"},
                   "head": {"ref": self.branch, "sha": self.head, "repo": {"full_name": self.repository}},
                   "base": {"ref": "dev", "sha": self.base, "repo": {"full_name": self.repository}}}
        self.files = [{"filename": "docs/images/performance/linux-x64/throughput.svg"},
                      {"filename": "docs/results/current/linux-x64/environment.json"}]

    def validate(self, pr=None, files=None):
        publisher.validate_results_pr(pr or self.pr, self.files if files is None else files,
                                      self.repository, self.branch, "dev", self.base, self.head)

    def test_exact_generated_bot_pr_accepted(self):
        self.validate()

    def test_source_head_fork_author_and_base_changes_rejected(self):
        for section, key, value in (("head", "sha", "c"*40), ("base", "sha", "c"*40),
                                    ("head", "ref", "other"), ("base", "ref", "main"),
                                    ("user", "login", "someone"), ("head", "repo", {"full_name": "fork/project"})):
            with self.subTest(section=section, key=key):
                pr = copy.deepcopy(self.pr)
                pr[section][key] = value
                with self.assertRaises(RuntimeError):
                    self.validate(pr=pr)

    def test_code_changes_and_empty_scope_rejected(self):
        for files in ([], self.files[:1] + [{"filename": "src/memory.cpp"}],
                      self.files[:1] + [{"filename": "README.md.backup"}],
                      self.files[:1] + [{"filename": "docs/performance/new-page.md"}]):
            with self.subTest(files=files), self.assertRaises(RuntimeError):
                self.validate(files=files)

    def test_exact_overview_files_allowed_without_similar_prefixes(self):
        for filename in ("README.md", "README.zh-CN.md", "docs/images/workload-comparison.png",
                         "docs/performance/latency.zh-CN.md"):
            self.validate(files=self.files[:1] + [{"filename": filename}])
        self.assertFalse(publisher.generated_path("README.md/memory.cpp"))
        self.assertFalse(publisher.generated_path("docs/images/workload-comparison.png.cpp"))

    def test_refresh_keeps_readme_user_prose_and_scenario_labels(self):
        before = "# Library\n\n## Performance\nWindows x64 · 2026-01-01\n| Operation | Native |\n| Create | 1.0 |\n\nSame user guidance.\n\n## Usage\nKeep this."
        after = before.replace("2026-01-01", "2026-10-11").replace("1.0", "0.8")
        publisher.validate_narrative(before, after, "README.md")
        for changed in (after.replace("Keep this", "Changed"), after.replace("Create", "Delete"),
                        after.replace("Same user guidance", "A different promise")):
            with self.assertRaises(RuntimeError):
                publisher.validate_narrative(before, changed, "README.md")

    def test_detail_refresh_only_changes_marked_data(self):
        before = "# Time\n<!-- performance-table:time -->\n| SDK | 1 |\n<!-- /performance-table -->\nUser guidance."
        after = before.replace("1", "2")
        publisher.validate_narrative(before, after, "docs/performance/latency.md")
        with self.assertRaises(RuntimeError):
            publisher.validate_narrative(before, after.replace("User guidance", "Different"), "docs/performance/latency.md")

    def test_only_real_pr_runs_for_exact_head_and_number_selected(self):
        valid = {"id": 1, "path": ".github/workflows/performance.yml", "event": "pull_request",
                 "head_sha": self.head, "head_repository": {"full_name": self.repository}, "pull_requests": [{"number": 4}]}
        runs = [valid, dict(valid, id=2), dict(valid, id=3, event="workflow_dispatch"),
                dict(valid, id=4, head_sha="c"*40), dict(valid, id=5, pull_requests=[{"number": 5}]),
                dict(valid, id=6, path=".github/workflows/unrelated.yml"),
                dict(valid, id=7, path="other/performance.yml"), dict(valid, id=8, head_repository={"full_name": "fork/project"}),
                dict(valid, id=9, path=".github/workflows/ci.yml")]
        selected = publisher.select_pr_runs(runs, 4, self.head, self.repository)
        self.assertEqual(set(selected), {"performance.yml"})
        self.assertEqual(selected["performance.yml"]["id"], 2)

    def test_incomplete_file_pagination_rejected(self):
        with self.assertRaises(RuntimeError):
            self.validate(files=self.files[:1])

    def test_file_pages_are_flattened_without_incompatible_cli_filters(self):
        with mock.patch.object(publisher, "run", return_value='[[{"filename":"README.md"}],[{"filename":"docs/results/current/data.csv"}]]') as command:
            files = publisher.read_pr_files(self.repository, 4)
        self.assertEqual([item["filename"] for item in files], ["README.md", "docs/results/current/data.csv"])
        command.assert_called_once_with("gh", "api", "--paginate", "--slurp", "repos/example/project/pulls/4/files")

    def test_publication_retains_all_raw_observations_losslessly(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            files = {"full.csv": b"trial,value\n1,2\n10,99\n", "summary.json": b'{"n":10}\n',
                     "controlled-suite.json": b'{"order":[2,1]}\n', "environment.json": b'{"schema":3}\n'}
            for name, content in files.items():
                (root / name).write_bytes(content)
            publisher.compress_results(root)
            first = (root / "full.csv.gz").read_bytes()
            for name, content in files.items():
                if name == "environment.json":
                    self.assertEqual((root / name).read_bytes(), content)
                else:
                    self.assertFalse((root / name).exists())
                    self.assertEqual(gzip.decompress((root / (name + ".gz")).read_bytes()), content)
            (root / "full.csv").write_bytes(files["full.csv"])
            publisher.compress_results(root)
            self.assertEqual((root / "full.csv.gz").read_bytes(), first)


if __name__ == "__main__":
    unittest.main()
