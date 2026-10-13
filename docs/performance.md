# Performance report

Historical figures and current local Windows validation are separate datasets; current Heap and statistics have not been measured across platforms.

[UniMemory](../README.md) · **English** · [简体中文](performance.zh-CN.md)

## Overview

Sections 1–6 and the common-workload figure use historical CI data: 2026-10-11 UTC, source `8365472eea66997cde5163182aef82d3b94dcc48`, [workflow 38167739252](https://github.com/dugan-dev/UniMemory/actions/runs/38167739252). Each SVG embeds provenance and aggregation. Historical `api_lookup` retrieved the former Global singleton per call; `api_cached` reused a reference obtained before timing. Neither measures the current directly constructed Global contract.

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

## 7 · Current Windows Native/API timings and acceptance

Windows x64 · MSVC 19.44 · C++20 · Release · /O2 /Ob3 · statistics off · 2026-10-13. Recorded profile: Release IPO=ON, diagnostic target IPO=OFF. Ten independent-process trials per scenario, alternating Native/API order; 125 ms calibration target. All samples are retained.

This measurement uses directly constructed Global contexts, with construction outside timing. Before-migration reference: `69651a1885c3417fa2f0751f04d3e6ecdea6cf25`. Both builds use the same timing kernels, SDKs and compiler settings; this is local evidence, not remote CI validation.

Median durations in ns; lower is better. `std`, `mi`, `je` denote standard, mimalloc, jemalloc Native paths; `-unim` denotes the matching API. Raw pairs use 16-byte alignment and include deallocation; other rows cover complete operations. Vector includes reserve(256), 256 uint64_t push_back calls and destruction.

| Scenario | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Allocate + free 16 B | 37.32 | 38.21 | 7.83 | 8.66 | 36.77 | 35.55 |
| Allocate + free 64 B | 38.75 | 38.36 | 8.34 | 9.23 | 34.16 | 33.85 |
| Allocate + free 256 B | 35.17 | 36.45 | 9.53 | 10.50 | 32.58 | 32.56 |
| Allocate + free 4 KiB | 36.02 | 36.71 | 28.24 | 29.52 | 41.09 | 39.75 |
| Allocate + free 64 KiB | 122.42 | 121.44 | 30.84 | 32.68 | 315.83 | 311.45 |
| Zeroed allocation | 38.59 | 39.53 | 12.29 | 12.60 | 35.84 | 37.13 |
| Reallocate | 76.01 | 76.82 | 18.42 | 19.12 | 100.69 | 101.53 |
| Reallocate with zeroed growth | 81.96 | 79.62 | 19.03 | 19.21 | 107.53 | 106.79 |
| Unique object, 64 B | 38.51 | 37.66 | 7.72 | 7.34 | 34.31 | 33.40 |
| Object array | 54.86 | 53.76 | 24.24 | 25.03 | 47.75 | 47.68 |
| Shared object | 69.86 | 71.21 | 37.62 | 37.95 | 69.75 | 69.70 |
| Vector, 256 64-bit integers | 244.67 | 302.94 | 279.84 | 265.72 | 310.14 | 311.26 |
| PMR Vector, 256 64-bit integers | 825.68 | 695.57 | 717.26 | 678.86 | 787.95 | 829.45 |
| OwnedBlock, 64 B | 40.33 | 41.03 | 6.22 | 7.01 | 36.38 | 36.18 |

### Compared with before migration

| Backend | Faster / total scenarios | Equal-weight geometric mean speedup |
| --- | ---: | ---: |
| standard | 8 / 14 | 2.1% |
| mimalloc | 13 / 14 | 6.6% |
| jemalloc | 7 / 14 | -2.9% |

Actual median durations improve in 28 of 42 scenarios; small differences may be measurement noise.

### Remaining cases below the 90% native threshold

The gate checks both paired medians and ratios of arithmetic mean durations; 39/42 pass. All failing results are retained and the threshold is unchanged.

| Backend | Scenario | Paired median | Mean-duration ratio |
| --- | --- | ---: | ---: |
| standard | Vector, 256 64-bit integers | 81.18% | 79.94% |
| mimalloc | Allocate + free 16 B | 88.78% | 89.56% |
| mimalloc | OwnedBlock, 64 B | 88.23% | 88.55% |

### Correctness

Local migration validation: 5,337 source CTest cases and 54 installed-consumer CTest cases passed.

Source: `docs/results/windows-local-comparison.json`; measured current source is the local working tree on 2026-10-13, with no fixed current commit recorded. Single-CPU affinity; context construction is excluded.

## 8 · Linux native applications: elapsed time and peak memory

Historical CI data: 2026-10-11 UTC, source `8365472eea66997cde5163182aef82d3b94dcc48`, same workflow. Sixteen standalone programs × three allocators, ten fresh processes each; threaded workloads use four workers. mimalloc-bench is pinned to `ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86`. Process-wide allocator replacement measures native allocator behavior rather than UniMemory wrapper overhead.

Three workloads are selected below; each cell is arithmetic mean [minimum, maximum]. Elapsed seconds include startup; peak RSS is measured by GNU time in MiB, unlike the phase snapshots above. Lower is better for both.

| Program / metric | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac / elapsed s | 3.679 [3.672, 3.725] | 3.339 [3.323, 3.477] | 3.433 [3.422, 3.522] |
| cfrac / peak MiB | 3.26 [3.12, 3.36] | 7.01 [6.96, 7.13] | 10.72 [10.63, 10.82] |
| malloc-large / elapsed s | 1.268 [1.218, 1.369] | 1.175 [1.118, 1.268] | 3.474 [3.424, 3.625] |
| malloc-large / peak MiB | 522.03 [521.93, 522.18] | 625.18 [625.16, 625.24] | 448.43 [446.62, 450.64] |
| glibc-simple / elapsed s | 2.219 [2.219, 2.220] | 1.619 [1.617, 1.620] | 2.024 [2.018, 2.069] |
| glibc-simple / peak MiB | 1.97 [1.97, 1.97] | 6.30 [6.27, 6.34] | 14.78 [14.38, 16.64] |

Larson, larson-sized and xmalloc-test have configured durations; elapsed times are excluded from comparisons. Raw records remain in `docs/results/current/native-applications/`; this selection does not rank the full sixteen-program suite.

## Data and method

### Historical CI environments (2026-10-11 UTC)

| Platform | CPU / logical CPUs | Compiler | Resident memory |
| --- | --- | --- | --- |
| Linux x64 | AMD EPYC 9V74 / 4 | GCC 13.3.0 | smaps_rollup RSS |
| Windows x64 | Intel Family 6 Model 207 / 4 | MSVC 19.44.35229.0 | Working set |
| macOS ARM64 | Apple M1 (Virtual) / 3 | AppleClang 17.0.0 | Mach resident_size |

All backend and statistics OFF/ON profiles on each platform share one controlled host and SDK binaries. Release builds warm each path, select counts with three sustained calibration pilots, alternate execution order and retain at least ten independent processes. Native/API counts match within each profile; different profiles may use different bounded counts. Oversubscribed scaling is capped at 32 rounds; counts beyond host capacity include scheduling and synchronization pressure.

Throughput, memory and common-operation figures use arithmetic means of process trials; tail-latency figures use arithmetic means of per-trial P99 values, including clock overhead. Memory trials subtract their baseline before aggregation. Statistics OFF/ON are separate runs. All samples are retained; summaries include mean, median, minimum/maximum, sample SD, CV and bootstrap 95% intervals. Small differences or high variation do not establish a reliable improvement.

Historical environments and data remain in `docs/results/current/<platform>/<backend>/<OFF|ON>/`; SVG metadata identifies exact files, protocol and source. Process trials remain in the repository; individual-operation traces and build evidence follow the workflow-artifact 90-day retention policy. Former data cannot establish current Heap, statistics or Global-construction performance, or rank backends across different machines.

### Local reproduction and acceptance

```sh
cmake -S . -B build/performance -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_PERFORMANCE_TESTS=ON
cmake --build build/performance --config Release --target UniMemoryPerformanceAcceptance
```

Build every backend/statistics combination separately. The performance target times serially, apart from CTest correctness checks. Default calibration is 250 ms; section 7 used 125 ms. At least ten alternating process trials must reach 90% Native for both paired medians and ratios of arithmetic mean durations; the threshold can only tighten. Read-only `--profile` verifies compiled settings, statistics and checks; sanitizer/coverage builds are rejected. Native/API must share SDK, CRT, optimization and IPO policy. IPO defaults off and is not imposed on installed consumers.

Run on an idle machine with a fresh output directory. Windows/Linux require CPU affinity; macOS runs unpinned and records the limitation. Insufficient calibration fails with evidence retained; no outliers are removed. Correctness acceptance covers independent modules, combined use and installed consumers. The 5,337/54 passes in section 7 are existing local migration records, rather than tests rerun for this documentation edit.
