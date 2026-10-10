import copy
import importlib.util
from pathlib import Path
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
                      self.files[:1] + [{"filename": "README.md"}],
                      self.files[:1] + [{"filename": "README.zh-CN.md"}]):
            with self.subTest(files=files), self.assertRaises(RuntimeError):
                self.validate(files=files)

    def test_only_real_pr_runs_for_exact_head_and_number_selected(self):
        valid = {"id": 1, "path": ".github/workflows/ci.yml", "event": "pull_request",
                 "head_sha": self.head, "head_repository": {"full_name": self.repository}, "pull_requests": [{"number": 4}]}
        runs = [valid, dict(valid, id=2), dict(valid, id=3, event="workflow_dispatch"),
                dict(valid, id=4, head_sha="c"*40), dict(valid, id=5, pull_requests=[{"number": 5}]),
                dict(valid, id=6, path=".github/workflows/unrelated.yml"),
                dict(valid, id=7, path="other/ci.yml"), dict(valid, id=8, head_repository={"full_name": "fork/project"})]
        selected = publisher.select_pr_runs(runs, 4, self.head, self.repository)
        self.assertEqual(set(selected), {"ci.yml"})
        self.assertEqual(selected["ci.yml"]["id"], 2)

    def test_incomplete_file_pagination_rejected(self):
        with self.assertRaises(RuntimeError):
            self.validate(files=self.files[:1])

    def test_file_pages_are_flattened_without_incompatible_cli_filters(self):
        with mock.patch.object(publisher, "run", return_value='[[{"filename":"README.md"}],[{"filename":"docs/results/current/data.csv"}]]') as command:
            files = publisher.read_pr_files(self.repository, 4)
        self.assertEqual([item["filename"] for item in files], ["README.md", "docs/results/current/data.csv"])
        command.assert_called_once_with("gh", "api", "--paginate", "--slurp", "repos/example/project/pulls/4/files")


if __name__ == "__main__":
    unittest.main()
