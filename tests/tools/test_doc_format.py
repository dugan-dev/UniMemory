import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_docs", ROOT / "tools/check-docs.py")
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)
CALIBRATION = Path("docs/results/current/windows-x64/standard/OFF/sweep-operations.txt")


class DocumentFormatTests(unittest.TestCase):
    def check_file(self, root, relative, data):
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        errors = []
        checker.check_text_format(path, root, errors)
        checker.check_publication_content(path, root, errors)
        self.assertEqual(path.read_bytes(), data)
        return errors

    def test_calibration_crlf_preserves_bytes_and_sha(self):
        data = b"raw:64:16:1:no,32768\r\nzeroed:4096:16:1:no,4096\r\n"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            digest = hashlib.sha256(data).hexdigest()
            self.assertEqual(self.check_file(root, CALIBRATION, data), [])
            self.assertEqual(hashlib.sha256((root / CALIBRATION).read_bytes()).hexdigest(), digest)

    def test_only_current_calibration_paths_allow_crlf(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in ("core/example.cpp", "docs/guide.md", "sweep-operations.txt",
                             "docs/results/current/notes.txt",
                             "docs/results/archive/windows-x64/sweep-operations.txt"):
                with self.subTest(path=relative):
                    errors = self.check_file(root, relative, b"value\r\n")
                    self.assertTrue(any("use LF line endings" in error for error in errors))

    def test_calibration_rejects_bare_cr_and_invalid_utf8(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for data, message in ((b"raw,64\rother,32\n", "use LF line endings"),
                                  (b"\xff\r\n", "invalid UTF-8 text")):
                with self.subTest(message=message):
                    self.assertTrue(any(message in error for error in
                                        self.check_file(root, CALIBRATION, data)))

    def test_calibration_keeps_other_format_checks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for data, message in ((b"\xef\xbb\xbfraw,64\r\n", "BOM"),
                                  (b"raw,64 \r\n", "trailing whitespace"),
                                  (b"\traw,64\r\n", "spaces for indentation"),
                                  (b"raw,64", "missing final newline"),
                                  (b"raw,64\r\n\r\n", "extra blank lines"),
                                  (b"raw,64\r\n\r\n\r\n\r\nnext,32\r\n", "excessive consecutive")):
                with self.subTest(message=message):
                    self.assertTrue(any(message in error for error in
                                        self.check_file(root, CALIBRATION, data)))

    def test_calibration_still_scans_publication_secrets(self):
        with tempfile.TemporaryDirectory() as temporary:
            data = b"ghp_" + b"a" * 31 + b"\r\n"
            errors = self.check_file(Path(temporary), CALIBRATION, data)
            self.assertTrue(any("access token" in error for error in errors))

    def test_git_add_and_checkout_preserve_calibration_bytes(self):
        data = b"raw:64:16:1:no,32768\r\nzeroed:4096:16:1:no,4096\r\n"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            def git(*arguments):
                return subprocess.run(["git", *arguments], cwd=root, check=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout
            git("init", "-q")
            git("config", "core.autocrlf", "true")
            (root / ".gitattributes").write_bytes((ROOT / ".gitattributes").read_bytes())
            self.check_file(root, CALIBRATION, data)
            ordinary = root / "ordinary.txt"
            ordinary.write_bytes(b"ordinary\r\n")
            git("add", ".gitattributes", CALIBRATION.as_posix(), "ordinary.txt")
            self.assertEqual(git("show", ":" + CALIBRATION.as_posix()), data)
            self.assertEqual(git("show", ":ordinary.txt"), b"ordinary\n")
            (root / CALIBRATION).write_bytes(b"changed\n")
            ordinary.write_bytes(b"changed\n")
            git("checkout", "--", CALIBRATION.as_posix(), "ordinary.txt")
            self.assertEqual((root / CALIBRATION).read_bytes(), data)
            self.assertEqual(hashlib.sha256((root / CALIBRATION).read_bytes()).digest(),
                             hashlib.sha256(data).digest())
            self.assertEqual(ordinary.read_bytes(), b"ordinary\n")


if __name__ == "__main__":
    unittest.main()
