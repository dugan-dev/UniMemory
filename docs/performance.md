# Performance report

[UniMemory](../README.md) · **English** · [简体中文](performance.zh-CN.md)

Compare time and memory by usage scenario. Results apply to the measured configurations.

| Topic | Measures |
| --- | --- |
| [1 · Time](performance/latency.md) | Objects, containers, resize, cross-thread use and statistics |
| [2 · Memory](performance/memory.md) | Process memory, retention, Heap collection and type sizes |
| [3 · Native programs](performance/applications.md) | Standalone native allocator time and peak memory; no UniMemory layer |

## Environment

| Item | Configuration |
| --- | --- |
| Version / measured date (UTC) | 0.0.1 / 2026-09-28 |
| CPU | Xeon w9-3595X, 120 logical CPUs |
| Windows | x64, MSVC 19.44, Release, no LTO |
| Linux | Ubuntu 24.04 / WSL2, GCC 13.3, Release, no LTO |
| Backend | mimalloc 3.4.3; jemalloc 5.3.1 |
| Statistics | Disabled unless labeled Basic |

## Coverage

| Probe | Scope |
| --- | --- |
| Allocation/free | Native, counters off and counters on; five sizes |
| API scenarios | 1001 per platform across 17 workloads |
| Memory usage | Mixed sizes and lifetimes; 270 phase snapshots |
| Native programs | 16 programs × 3 native allocators × 3 trials |

## Interpretation

| Observation | Meaning |
| --- | --- |
| API versus native | The unified interface adds call overhead |
| Statistics | Enabled counters add work; disabled counters do not update |
| Backend choice | Allocation speed and memory retention involve tradeoffs |

CPU affinity is unpinned; small differences may be noise. No p95/p99, NUMA, long-running fragmentation, macOS or mobile performance claim is made.

[Method](benchmarking.md) · [Raw data](results/0.0.1/README.md) · [Tests](testing.md)
