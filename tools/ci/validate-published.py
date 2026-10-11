"""Validate tracked current results in documentation-only bot PRs."""
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("report", root / "tools/ci/performance-report.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
directories = [root / "docs/results/current" / platform / backend / mode / "environment.json"
               for platform in ("linux-x64", "windows-x64", "macos-arm64")
               for backend in report.BACKENDS for mode in ("OFF", "ON")]
directories = [path for path in directories if path.exists()]
if not directories:
    print("No current performance baseline is published yet")
for manifest in directories:
    report.validate(manifest.parent)
    print("Validated published result:", manifest.parent.name)
native = root / "docs/results/current/native-applications"
if native.exists():
    spec = importlib.util.spec_from_file_location("tables", root / "tools/ci/refresh-performance-tables.py")
    tables = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tables)
    tables.validate_native_data(native)
    print("Validated 144 native application runs")
