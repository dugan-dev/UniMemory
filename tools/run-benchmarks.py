"""Run Delivery latency/footprint/heap/pressure in fresh, retained process trials."""
import argparse
import json
from pathlib import Path
from benchmark_measurements import run_base, TRIALS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--trials', type=int, default=TRIALS)
    parser.add_argument('--backends', nargs='+', choices=('standard', 'mimalloc', 'jemalloc'))
    parser.add_argument('--calibration', type=Path)
    args = parser.parse_args()
    from benchmark_profile import read_profile
    if args.trials < 10:
        parser.error('--trials must provide at least ten independent process samples')
    if args.backends is not None and args.backends != [read_profile(args.executable)['backend']]:
        parser.error('--backends must match the single compiled backend')
    calibration = json.loads(args.calibration.read_text()) if args.calibration else None
    run_base(args.executable, args.output, args.trials, calibration=calibration)


if __name__ == '__main__':
    main()
