import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("configure", Path(__file__).resolve().parents[2] / "tools/ci/configure.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class Process:
    def __init__(self, text, result):
        self.stdout = io.StringIO(text)
        self.result = result

    def wait(self):
        return self.result


class ConfigureTests(unittest.TestCase):
    def test_network_failure_retried(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(module.subprocess, "Popen", side_effect=[Process("SSL connect error\n", 1), Process("Configured\n", 0)]) as spawn, patch.object(module.time, "sleep"):
            self.assertEqual(module.configure(["cmake"], Path(directory) / "log"), 0)
            self.assertEqual(spawn.call_count, 2)

    def test_compiler_failure_not_retried(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(module.subprocess, "Popen", return_value=Process("Compiler rejected source\n", 2)) as spawn:
            self.assertEqual(module.configure(["cmake"], Path(directory) / "log"), 2)
            self.assertEqual(spawn.call_count, 1)

    def test_persistent_network_failure_still_fails(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(module.subprocess, "Popen", side_effect=[Process("SSL connect error\n", 1) for _ in range(3)]) as spawn, patch.object(module.time, "sleep"):
            self.assertEqual(module.configure(["cmake"], Path(directory) / "log"), 1)
            self.assertEqual(spawn.call_count, 3)


if __name__ == "__main__":
    unittest.main()
