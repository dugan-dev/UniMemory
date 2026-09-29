"""Apply a pinned upstream test-only fix to jemalloc 5.3.1 validation sources."""
from pathlib import Path
import hashlib
import os
import subprocess
import sys
from urllib.request import urlopen


def main():
    source = Path(sys.argv[1]).resolve()
    revision = "1b022c0da70c0d9d259e9beab6fb7db91ae79567"
    expected = "f9e449db1ee0f30c002c6b1750eefee84d964a362e2b8a3135fc69834e8c800c"
    if not (source / "VERSION").read_text().startswith("5.3.1"):
        raise RuntimeError("The test fix requires jemalloc 5.3.1")
    url = f"https://github.com/jemalloc/jemalloc/commit/{revision}.patch"
    with urlopen(url, timeout=60) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != expected:
        raise RuntimeError("Upstream test patch checksum mismatch")
    patch = data.replace(b"jemalloc/internal/arena.h", b"jemalloc/internal/arena_types.h")
    environment = os.environ.copy()
    environment["GIT_CEILING_DIRECTORIES"] = str(source.parent)
    command = ["git", "apply"]
    check = subprocess.run(command + ["--check"], cwd=source, env=environment,
                           input=patch, capture_output=True)
    if check.returncode == 0:
        subprocess.run(command, cwd=source, env=environment, input=patch, check=True)
    else:
        subprocess.run(command + ["--reverse", "--check"], cwd=source, env=environment,
                       input=patch, check=True)
    extent = source / "test/integration/extent.c"
    text = extent.read_text()
    header = '#include "jemalloc/internal/pages.h"\n'
    if header not in text:
        anchor = '#include "test/extent_hooks.h"'
        if text.count(anchor) != 1:
            raise RuntimeError("Unexpected jemalloc test includes")
        extent.write_text(text.replace(anchor, anchor + "\n" + header.rstrip()))
    print(f"Applied upstream test fix {revision}; allocator sources unchanged")


if __name__ == "__main__":
    main()
