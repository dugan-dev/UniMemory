# Current measurements

[Performance](../../performance.md) · [简体中文](../../performance.zh-CN.md)

The latest measurements are grouped by platform, backend and statistics setting. Each environment file records its date, source revision and remote run.

| Data | Contents |
| --- | --- |
| `<platform>/<backend>/OFF` | Native calls and UniMemory with statistics off |
| `<platform>/<backend>/ON` | Native calls and UniMemory with statistics on |
| `full.csv` | Objects, containers, resizing, heaps and stack operations |
| `latency.csv`, `tails.csv` | Allocation/free timing and individual latency samples |
| `scaling.csv` | Same-thread and cross-thread workloads, 1–16 threads |
| `pressure.csv`, `footprint.csv` | Memory during allocation, release and collection |
| `environment.json`, `sweep-environment.json` | Hardware, compiler, SDK and measured executable provenance |
| [Native applications](native-applications/) | 144 runs across 16 standalone programs; native allocators only |

Each profile retains three process trials. Backends and statistics settings run separately; hardware and SDK builds may differ. Compare Native and UniMemory within the same profile when evaluating wrapper cost.
