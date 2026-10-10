"""Analyze project translation units using their actual compilation database."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("database", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
files = sorted({Path(entry["file"]).resolve() for entry in json.loads(args.database.read_text())
                if Path(entry["file"]).resolve().is_relative_to(root / "src")})
if not files:
    raise SystemExit("No project translation units to analyze")
for file in files:
    subprocess.run(["clang-tidy-18", str(file), "-p", str(args.database.parent),
                    "--checks=-*,clang-analyzer-*,bugprone-use-after-move",
                    "--warnings-as-errors=*", "--header-filter=.*[/\\\\]unimem[/\\\\].*"], check=True)
