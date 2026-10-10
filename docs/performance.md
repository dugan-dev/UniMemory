# Performance report

[UniMemory](../README.md) · **English** · [简体中文](performance.zh-CN.md)

Compare time and memory by usage scenario. Results apply to the measured configurations.

## Current remote reports

Each platform compares Standard, mimalloc and jemalloc native calls with UniMemory,
including statistics cost, three process trials and recorded machine capacity.
Use the analysis for source revision and interpretation; environment manifests
are included in the current measurements below.

| Platform | Analysis | Scaling | Memory | Latency | Statistics | Retention | Workloads |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Linux x64 | [Report](images/performance/linux-x64/README.md) | [Chart](images/performance/linux-x64/throughput.svg) | [Chart](images/performance/linux-x64/memory.svg) | [Chart](images/performance/linux-x64/latency.svg) | [Chart](images/performance/linux-x64/statistics.svg) | [Chart](images/performance/linux-x64/retention.svg) | [Chart](images/performance/linux-x64/workloads.svg) |
| Windows x64 | [Report](images/performance/windows-x64/README.md) | [Chart](images/performance/windows-x64/throughput.svg) | [Chart](images/performance/windows-x64/memory.svg) | [Chart](images/performance/windows-x64/latency.svg) | [Chart](images/performance/windows-x64/statistics.svg) | [Chart](images/performance/windows-x64/retention.svg) | [Chart](images/performance/windows-x64/workloads.svg) |
| macOS ARM64 | [Report](images/performance/macos-arm64/README.md) | [Chart](images/performance/macos-arm64/throughput.svg) | [Chart](images/performance/macos-arm64/memory.svg) | [Chart](images/performance/macos-arm64/latency.svg) | [Chart](images/performance/macos-arm64/statistics.svg) | [Chart](images/performance/macos-arm64/retention.svg) | [Chart](images/performance/macos-arm64/workloads.svg) |

[Current measurements](results/current/) include validated CSV and environment manifests.
Individual samples remain in the linked run's artifacts for 14 days; external
native workload results are also uploaded there. Aggregate data stays in the
repository. Hosted timing is a comparison signal, not an absolute speed gate.
[Method and automation](remote-validation.md#performance-and-charts)

## Historical report · 2026-09-28

These are historical measurements from **2026-09-28**, not new measurements of the October repairs. The source and executable hashes in the raw data identify the measured builds even though the library version remains 0.0.1.

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

CPU affinity is unpinned; small differences may be noise. This historical report makes no p95/p99, NUMA, long-running fragmentation, macOS or mobile performance claim.

There is no repository-wide latency or memory regression budget. Before evaluating a performance change, state the workload, baseline revision, native comparison, compiler/backend settings, acceptable variation and time/memory limits. Rerun that workload and retain raw trials. API fixtures and native benchmark programs do not establish production adoption or prove an optimal backend for an application.

[Method](benchmarking.md) · [Raw data](results/0.0.1/README.md) · [Tests](testing.md)
