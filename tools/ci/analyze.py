"""Analyze project translation units using their actual compilation database."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("database", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
# Valid header consumers exercise the implementation without intentionally-invalid
# unit-test calls becoming analyzer warnings (e.g. a moved-from memory fixture).
project_tus = (root / "tests/headers", root / "examples")
files = sorted({Path(entry["file"]).resolve() for entry in json.loads(args.database.read_text())
                if any(Path(entry["file"]).resolve().is_relative_to(folder) for folder in project_tus)})
if not files:
    raise SystemExit("No project translation units to analyze")
failures = []
for file in files:
    print(f"Analyzing {file.relative_to(root)}", flush=True)
    process = subprocess.run(["clang-tidy-18", str(file), "-p", str(args.database.parent),
                    "--checks=-*,clang-analyzer-*,bugprone-use-after-move",
                    "--warnings-as-errors=*", "--header-filter=.*[/\\\\]unimem[/\\\\].*"], check=False)
    if process.returncode:
        failures.append((file.relative_to(root), process.returncode))
if failures:
    summary = "\n".join(f"  {file}: exit {code}" for file, code in failures)
    raise SystemExit(f"Static analysis failed for {len(failures)}/{len(files)} translation units:\n{summary}")
print(f"Static analysis passed for all {len(files)} translation units", flush=True)
