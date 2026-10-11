"""Retry transient dependency downloads, never compiler or test failures."""
import argparse
from pathlib import Path
import re
import subprocess
import time

NETWORK = re.compile(r"SSL connect error|Could not resolve host|Operation timed out|curl operation failed|HTTP response code said error", re.I)


def configure(command, output, retries=3):
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as log:
        for attempt in range(retries):
            lines = []
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, encoding="utf-8", errors="replace")
            for line in process.stdout:
                print(line, end="", flush=True)
                log.write(line)
                lines.append(line)
            result = process.wait()
            log.flush()
            if result == 0 or not NETWORK.search("".join(lines)) or attempt + 1 == retries:
                return result
            print(f"Transient dependency download failure; retry {attempt+2}/{retries}", flush=True)
            time.sleep(10)
    raise RuntimeError("No configure attempt executed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("configure command required")
    raise SystemExit(configure(command, Path("build/configure.log")))
