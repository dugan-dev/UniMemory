# Performance report

[UniMemory](../README.md) · **English** · [简体中文](performance.zh-CN.md)

## Current remote reports

Compare Standard, mimalloc and jemalloc native allocation with UniMemory. Three fresh-process trials; medians shown. Compare each platform separately.

| Legend | Meaning |
| --- | --- |
| Native | Direct allocator calls |
| UniMemory | Unified interface, statistics disabled |
| API + stats | Unified interface, Basic statistics enabled |

Throughput charts record logical CPU capacity; thread counts above it include oversubscription. Each chart identifies its platform and measured revision.

## 1 · Throughput

**Allocation/free pairs per second as thread count increases; higher is better.** A 64-byte cross-thread release workload.

### Linux x64

![Linux cross-thread throughput](images/performance/linux-x64/throughput.svg)

### Windows x64

![Windows cross-thread throughput](images/performance/windows-x64/throughput.svg)

### macOS ARM64

![macOS cross-thread throughput](images/performance/macos-arm64/throughput.svg)

## 2 · Peak memory

**Resident memory under the same workload; lower is better.** Includes thread stacks, process and allocator state.

### Linux x64

![Linux peak resident memory](images/performance/linux-x64/memory.svg)

### Windows x64

![Windows peak resident memory](images/performance/windows-x64/memory.svg)

### macOS ARM64

![macOS peak resident memory](images/performance/macos-arm64/memory.svg)

## 3 · Tail latency

**Slow allocation requests at each size; lower is better.** P99 means 99% of samples are at or below this latency; clock overhead is included.

### Linux x64

![Linux individual allocation p99](images/performance/linux-x64/latency.svg)

### Windows x64

![Windows individual allocation p99](images/performance/windows-x64/latency.svg)

### macOS ARM64

![macOS individual allocation p99](images/performance/macos-arm64/latency.svg)

## 4 · Statistics cost

**Additional latency with counters enabled.** Compare within the same backend and size; patterned columns enable statistics.

### Linux x64

![Linux statistics cost](images/performance/linux-x64/statistics.svg)

### Windows x64

![Windows statistics cost](images/performance/windows-x64/statistics.svg)

### macOS ARM64

![macOS statistics cost](images/performance/macos-arm64/statistics.svg)

## 5 · Memory after release

**Memory across allocation, survival and release cycles.** The final phase has no live requests; retained resident memory may be cache, not a leak.

### Linux x64

![Linux mixed-lifetime retention](images/performance/linux-x64/retention.svg)

### Windows x64

![Windows mixed-lifetime retention](images/performance/windows-x64/retention.svg)

### macOS ARM64

![macOS mixed-lifetime retention](images/performance/macos-arm64/retention.svg)

## 6 · Common operations

**Relative time for objects, resizing and containers; lower is better.** Standard equals 1 within each group; different operations have different costs.

### Linux x64

![Linux normalized API workloads](images/performance/linux-x64/workloads.svg)

### Windows x64

![Windows normalized API workloads](images/performance/windows-x64/workloads.svg)

### macOS ARM64

![macOS normalized API workloads](images/performance/macos-arm64/workloads.svg)

## Historical report · 2026-09-28

The original Windows/Linux workload comparison is preserved. Standard equals 1; shorter bars are faster. Its environment differs from the current remote reports.

![Historical Windows and Linux comparison](images/workload-comparison.png)

[Historical time](performance/latency.md) · [Historical memory](performance/memory.md) · [Native applications](performance/applications.md)

## Data and method

[Current CSV and environment](results/current/) · [Historical data](results/0.0.1/README.md) · [Measurement method](benchmarking.md)

Source changes automatically update the figures. Individual samples remain in Actions artifacts for 14 days; aggregate data stays in the repository. Small hosted-machine variations are not absolute speed gates. These measurements do not establish long-term fragmentation, NUMA or mobile performance.
