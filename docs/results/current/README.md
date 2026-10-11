# Current measurements

[Performance](../../performance.md) · [简体中文](../../performance.zh-CN.md)

The latest measurements are grouped by platform, backend and statistics setting. Each environment file records its date, source revision and remote run.

| Data | Contents |
| --- | --- |
| `<platform>/<backend>/OFF` | Native calls and UniMemory with statistics off |
| `<platform>/<backend>/ON` | Native calls and UniMemory with statistics on |
| `full.csv.gz` | Objects, containers, resizing, heaps and stack operations |
| `latency.csv.gz`, `tails.csv.gz` | Allocation/free timing and individual latency samples |
| `scaling.csv.gz` | Same-thread and cross-thread workloads, 1–16 threads |
| `pressure.csv.gz`, `footprint.csv.gz` | Memory during allocation, release and collection |
| `environment.json`, `sweep-environment.json` | Hardware, compiler, SDK and measured executable provenance |
| `summary.json.gz`, `controlled-suite.json.gz` | Independent-trial statistics and the controlled measurement schedule |
| `calibration-pilots.json.gz`, `full-calibration.json.gz` | Sustained pilot observations, limits and selected operation counts |
| [Native applications](native-applications/) | 16 standalone programs; native allocators only |

All process-trial observations are retained losslessly. Individual operation traces and build evidence are available in the linked workflow artifacts for 90 days under the current protocol. Check each environment file for the measured source, protocol and trial count. The current protocol runs every backend on the same platform runner, with at least ten independent process trials; legacy measurements explicitly retain their original protocol.
