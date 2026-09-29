"""Check text format, delivery links and English source/examples with Python stdlib."""
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit


def check_text_format(path, root, errors):
    data = path.read_bytes()
    name = path.relative_to(root)
    try:
        content = data.decode("utf-8")
    except UnicodeDecodeError:
        errors.append(f"{name}: invalid UTF-8 text")
        return
    if data.startswith(b"\xef\xbb\xbf"):
        errors.append(f"{name}: UTF-8 BOM is not allowed")
    if b"\r" in data:
        errors.append(f"{name}: use LF line endings")
    if content and not content.endswith("\n"):
        errors.append(f"{name}: missing final newline")
    if re.search(r"\n[ \t]*\n\Z", content):
        errors.append(f"{name}: extra blank lines at end of file")
    if re.search(r"\n(?:[ \t]*\n){3,}", content):
        errors.append(f"{name}: excessive consecutive blank lines")
    if path.suffix != ".md":
        for number, line in enumerate(content.splitlines(), 1):
            if line.rstrip(" \t") != line:
                errors.append(f"{name}:{number}: trailing whitespace")
            if re.match(r"[ \t]*\t", line):
                errors.append(f"{name}:{number}: use spaces for indentation")


def heading_ids(text):
    ids = set()
    counts = {}
    for title in re.findall(r"^#{1,6}\s+(.+)$", text, re.MULTILINE):
        slug = re.sub(r"[^\w\- ]", "", title.lower().strip()).replace(" ", "-")
        number = counts.get(slug, 0)
        counts[slug] = number + 1
        ids.add(f"{slug}-{number}" if number else slug)
    return ids


def check_publication_content(path, root, errors):
    content = path.read_bytes()
    patterns = {
        "local user path": rb"(?:[A-Z]:[\\/]+(?:Users|Documents and Settings)[\\/]+|/(?:home|Users)/[A-Za-z0-9_.-]+/)",
        "private key": rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
        "access token": rb"(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|sk-(?:proj-|ant-)?[A-Za-z0-9_-]{30,}|AKIA[0-9A-Z]{16})",
        "URL credential": rb"https?://[^\s/:]+:[^\s/@]+@",
    }
    for label, pattern in patterns.items():
        if re.search(pattern, content, re.IGNORECASE):
            errors.append(f"{path.relative_to(root)}: publication check found {label}")


def main():
    root = Path(__file__).resolve().parents[1]
    names = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root).decode("utf-8").split("\0")
    paths = [root / name for name in set(names) if name and (root / name).is_file()]
    errors = []
    text_paths = [path for path in paths if path.suffix not in {".png", ".zip"}]
    for path in text_paths:
        check_text_format(path, root, errors)
        check_publication_content(path, root, errors)
    docs = [path for path in paths if path.suffix == ".md"]
    for path in docs:
        content = path.read_text(encoding="utf-8")
        for target in re.findall(r"\]\(([^)]+)\)", content):
            target = target.strip().strip("<>")
            url = urlsplit(target)
            if url.scheme or target.startswith("//"):
                continue
            resolved = (path.parent / unquote(url.path)).resolve() if url.path else path
            if not resolved.exists():
                errors.append(f"{path.relative_to(root)}: missing link {target}")
            elif url.fragment and resolved.suffix == ".md":
                if unquote(url.fragment) not in heading_ids(resolved.read_text(encoding="utf-8")):
                    errors.append(f"{path.relative_to(root)}: missing heading {target}")
        for language, block in re.findall(r"```([^\n]*)\n(.*?)```", content, re.DOTALL):
            if language.strip() in {"cpp", "c++", "c", "cmake", "sh", "bash", "powershell", "python", "json"}:
                if re.search(r"[\u3400-\u9fff]", block):
                    errors.append(f"{path.relative_to(root)}: non-English code example")
    suffixes = {".h", ".inl", ".cpp", ".cmake", ".in", ".py", ".ps1", ".sh", ".json", ".yml", ".yaml", ".bazel"}
    sources = [path for path in paths if path.suffix in suffixes or path.name == "CMakeLists.txt"]
    for path in sources:
        content = path.read_text(encoding="utf-8")
        if re.search(r"[\u3400-\u9fff]", content):
            errors.append(f"{path.relative_to(root)}: non-English source/configuration text")
        if path.suffix == ".py":
            try:
                compile(content, str(path), "exec")
            except SyntaxError as error:
                errors.append(f"{path.relative_to(root)}: {error}")
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print(f"Checked {len(text_paths)} text files, {len(docs)} Markdown files "
          f"and {len(sources)} source/configuration files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
