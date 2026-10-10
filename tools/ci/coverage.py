"""Collect GCC JSON line/branch coverage for project sources and public headers."""
import argparse
import gzip
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
args.output.mkdir(parents=True, exist_ok=True)
records = {}
objects = list(args.build.rglob("*.gcno"))
if not objects or not list(args.build.rglob("*.gcda")):
    raise SystemExit("No executed coverage data")
for obj in objects:
    subprocess.run(["gcov-13", "--json-format", "--branch-probabilities", str(obj.resolve())],
                   cwd=args.output, check=True, stdout=subprocess.DEVNULL)
    for report in args.output.glob("*.gcov.json.gz"):
        data = json.loads(gzip.decompress(report.read_bytes()))
        for file in data["files"]:
            path = Path(file["file"])
            if not path.is_absolute():
                path = Path(data["current_working_directory"]) / path
            try:
                relative = path.resolve().relative_to(root).as_posix()
            except ValueError:
                continue
            if not relative.startswith(("src/", "include/")):
                continue
            lines = records.setdefault(relative, {})
            for line in file["lines"]:
                number = line["line_number"]
                record = lines.setdefault(number, {"count": 0, "branches": {}})
                record["count"] += line["count"]
                for index, branch in enumerate(line.get("branches", [])):
                    record["branches"][index] = record["branches"].get(index, 0) + branch["count"]
        report.unlink()
if not records:
    raise SystemExit("Coverage did not contain project code")
summary = {}
for file, lines in sorted(records.items()):
    branches = [count for line in lines.values() for count in line["branches"].values()]
    summary[file] = {"lines": len(lines), "covered_lines": sum(line["count"] > 0 for line in lines.values()),
                     "branches": len(branches), "covered_branches": sum(count > 0 for count in branches),
                     "uncovered_lines": sorted(number for number, line in lines.items() if line["count"] == 0)}
text = json.dumps(summary, indent=2) + "\n"
(args.output / "coverage.json").write_text(text, encoding="utf-8")
print(text)
