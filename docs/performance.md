# Performance report

[UniMemory](../README.md) · **English** · [简体中文](performance.zh-CN.md)

## Overview

Compare results within each workload. Each figure records its measured source and aggregation method; detailed tables include variation across independent process trials.

| Legend | Meaning |
| --- | --- |
| Native | Direct allocator calls |
| UniMemory | Statistics off |
| API + stats | Statistics on |

## 1 · Throughput

**Allocation/free pairs per second as thread count increases; higher is better.** A 64-byte cross-thread release workload.

### Linux x64

![Linux cross-thread throughput](images/performance/linux-x64/throughput.svg)

### Windows x64

![Windows cross-thread throughput](images/performance/windows-x64/throughput.svg)

### macOS

![macOS cross-thread throughput](images/performance/macos-arm64/throughput.svg)

## 2 · Peak memory

**Resident memory under the same workload; lower is better.** Includes thread stacks, process and allocator state.

### Linux x64

![Linux peak resident memory](images/performance/linux-x64/memory.svg)

### Windows x64

![Windows peak resident memory](images/performance/windows-x64/memory.svg)

### macOS

![macOS peak resident memory](images/performance/macos-arm64/memory.svg)

## 3 · Tail latency

**Slow allocation requests at each size; lower is better.** P99 means 99% of samples are at or below this latency; clock overhead is included.

### Linux x64

![Linux individual allocation p99](images/performance/linux-x64/latency.svg)

### Windows x64

![Windows individual allocation p99](images/performance/windows-x64/latency.svg)

### macOS

![macOS individual allocation p99](images/performance/macos-arm64/latency.svg)

## 4 · Statistics cost

**Allocation latency with statistics off or on; lower is better.** Patterned columns enable statistics. These are separate runs, not a controlled difference measurement.

### Linux x64

![Linux statistics cost](images/performance/linux-x64/statistics.svg)

### Windows x64

![Windows statistics cost](images/performance/windows-x64/statistics.svg)

### macOS

![macOS statistics cost](images/performance/macos-arm64/statistics.svg)

## 5 · Memory after release

**Memory across allocation, survival and release cycles.** The final phase has no live requests; retained resident memory may be cache, not a leak.

### Linux x64

![Linux mixed-lifetime retention](images/performance/linux-x64/retention.svg)

### Windows x64

![Windows mixed-lifetime retention](images/performance/windows-x64/retention.svg)

### macOS

![macOS mixed-lifetime retention](images/performance/macos-arm64/retention.svg)

## 6 · Common operations

**Relative time for objects, resizing and containers; lower is better.** Standard equals 1 within each group; different operations have different costs.

### Linux x64

![Linux normalized API workloads](images/performance/linux-x64/workloads.svg)

### Windows x64

![Windows normalized API workloads](images/performance/windows-x64/workloads.svg)

### macOS

![macOS normalized API workloads](images/performance/macos-arm64/workloads.svg)

## Common workloads

Standard = 1 within each workload; shorter bars are faster.

![Windows and Linux workload comparison](images/workload-comparison.png)

[Operation timings](performance/latency.md) · [Memory usage](performance/memory.md) · [Native applications](performance/applications.md)

## Data and method

[Measurements and environment](results/current/) · [Method](benchmarking.md)

Figures update with new measurements. Performance varies with hardware and workload.
