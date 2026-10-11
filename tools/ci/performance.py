"""Retain matched Native/API scaling, tails and latency in fresh process trials."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from benchmark_measurements import (run_base, run_scaling_tails, calibrate_delivery,
                                    finalize_traces, measure, percentile, write_rows, TRIALS, SIZES, THREADS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--trials', type=int, default=TRIALS)
    parser.add_argument('--calibration', type=Path)
    args = parser.parse_args()
    if args.trials < 10:
        parser.error('--trials must provide at least ten independent process samples')
    calibration = json.loads(args.calibration.read_text()) if args.calibration else calibrate_delivery(args.executable)
    run_base(args.executable, args.output, args.trials, calibration=calibration)
    run_scaling_tails(args.executable, args.output, args.trials, calibration=calibration)
    finalize_traces(args.output)


if __name__ == '__main__':
    main()
