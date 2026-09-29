# Performance report

[UniMemory](../README.md) · **English** · [简体中文](performance.zh-CN.md)

Compare time, memory and native programs separately. Results apply to the measured workloads and configurations.

| Topic | Measures |
| --- | --- |
| [1 · Time](performance/latency.md) | Object, Container, resize, handoff, Native/API/Basic |
| [2 · Memory](performance/memory.md) | RSS, retention, size rounding, Heap collect, type sizes |
| [3 · Native programs](performance/applications.md) | Standalone native allocator time and peak memory; no UniMemory layer |

## Environment

| Item | Configuration |
| --- | --- |
| Version / measured date (UTC) | 0.0.1 / 2026-09-28 |
| CPU | Xeon w9-3595X, 120 logical CPUs |
| Windows | x64, MSVC 19.44, Release, no LTO |
| Linux | Ubuntu 24.04 / WSL2, GCC 13.3, Release, no LTO |
| Backend | mimalloc 3.4.3; jemalloc 5.3.1 |
| Isolation | mimalloc override/CRT redirect off; Unix jemalloc je_ + --disable-cxx |
| Statistics | Disabled unless labeled Basic |

## Coverage

| Probe | Scope |
| --- | --- |
| Native / API / Basic | 2 OS × 3 Backends × 3 paths × 5 sizes × 3 process trials |
| API sweep | 1001 scenarios / OS, 17 workloads, 3 process trials, 7 repetitions |
| Mixed-size memory | 54 process trials, 270 phase snapshots, content/alignment/count checks |
| Native programs | 16 programs × 3 native allocators × 3 trials |

## Interpretation

| Observation | Meaning |
| --- | --- |
| API adds cost | Validation and runtime backend dispatch add work |
| Basic adds time | Atomic counters execute only when enabled |
| Faster need not use less memory | Caches and reclamation policy trade time against retention |

CPU affinity is unpinned; small differences may be noise. No p95/p99, NUMA, long-running fragmentation, macOS or mobile performance claim is made.

[Method](benchmarking.md) · [Raw data](results/0.0.1/README.md) · [Tests](testing.md)
