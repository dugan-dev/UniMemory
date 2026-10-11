"""Refresh aggregate and profile SVGs from the complete recorded source matrix."""
import argparse
import importlib.util
import json
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current", type=Path, default=root / "docs/results/current")
    parser.add_argument("--images", type=Path, default=root / "docs/images/performance")
    parser.add_argument("--source-revision", help="Reject measurements from any other source revision")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("performance_report", root / "tools/ci/performance-report.py")
    report = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(report)
    print(json.dumps(report.refresh_reports(args.current, args.images, args.source_revision), sort_keys=True))


if __name__ == "__main__":
    main()
