"""Validate tracked current results in documentation-only bot PRs."""
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("report", root / "tools/ci/performance-report.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
directories = sorted((root / "docs/results/current").rglob("environment.json"))
if not directories:
    print("No current performance baseline is published yet")
for manifest in directories:
    report.validate(manifest.parent)
    print("Validated published result:", manifest.parent.name)
