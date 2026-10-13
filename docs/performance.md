# Performance report

Performance informs comparison and optimization, not correctness or release eligibility. The 90% ratio is advisory; historical pass/fail records retain their original policy.

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

## 7 · Latest three-platform Native/API comparison

2026-10-13 · measured source `4f98078` · Actions `38372589028`. Three-platform correctness passed; 106/126 core performance cases passed. Windows and macOS did not meet every threshold.

Global core API, Release, statistics off, ten paired trials. Tables show median ns; lower is faster. FAIL means the paired-median or arithmetic-mean ratio was below 90% of Native. Acceptance is not determined by dividing the displayed medians.

### Windows · MSVC 19.44.35229.0

| Scenario | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Allocate/free 16 B | 32.89 | 33.40 | 5.15 | 5.22 FAIL | 35.14 | 42.78 FAIL |
| Allocate/free 64 B | 32.87 | 32.89 | 5.12 | 5.17 | 37.30 | 41.78 FAIL |
| Allocate/free 256 B | 33.30 | 33.61 | 5.79 | 6.00 | 35.22 | 41.29 FAIL |
| Allocate/free 4 KiB | 33.36 | 33.61 | 20.68 | 20.94 | 35.44 | 39.90 FAIL |
| Allocate/free 64 KiB | 104.63 | 106.53 | 22.86 | 23.02 | 218.03 | 221.29 |
| Zeroed allocation | 35.12 | 35.96 | 10.51 | 11.18 | 38.08 | 45.54 FAIL |
| Reallocate | 70.01 | 69.97 | 19.31 | 19.22 | 89.56 | 90.34 |
| Reallocate with zeroed growth | 70.40 | 71.12 | 19.28 | 19.81 | 90.55 | 91.05 |
| Unique object | 35.81 | 34.95 | 7.72 | 7.31 | 33.77 | 34.00 |
| Object array | 49.11 | 48.84 | 22.07 | 21.87 | 46.55 | 47.88 |
| Shared object | 40.86 | 42.82 | 12.62 | 13.69 | 56.13 | 58.42 |
| Vector | 240.58 | 242.15 | 226.01 | 229.18 | 250.63 | 240.97 |
| PMR Vector | 866.72 | 868.49 | 848.51 | 849.24 | 868.69 | 873.72 |
| OwnedBlock | 34.02 | 34.30 | 4.81 | 5.34 | 46.13 | 49.94 |

36/42 cases passed.

### Linux · GNU 13.3.0

| Scenario | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Allocate/free 16 B | 11.38 | 11.07 | 7.96 | 7.99 | 15.35 | 15.35 |
| Allocate/free 64 B | 11.37 | 11.05 | 8.25 | 8.21 | 15.36 | 15.37 |
| Allocate/free 256 B | 11.37 | 11.07 | 8.83 | 8.83 | 15.60 | 15.60 |
| Allocate/free 4 KiB | 23.08 | 22.64 | 27.44 | 28.12 | 18.91 | 18.87 |
| Allocate/free 64 KiB | 25.47 | 25.26 | 29.31 | 29.50 | 319.82 | 320.92 |
| Zeroed allocation | 12.43 | 12.50 | 11.21 | 10.91 | 19.31 | 18.85 |
| Reallocate | 24.61 | 25.36 | 20.91 | 21.84 | 46.99 | 47.21 |
| Reallocate with zeroed growth | 24.21 | 24.59 | 22.50 | 22.23 | 48.10 | 48.09 |
| Unique object | 27.08 | 11.17 | 20.43 | 7.54 | 26.30 | 13.39 |
| Object array | 69.87 | 25.18 | 33.54 | 25.59 | 38.61 | 24.36 |
| Shared object | 15.70 | 16.55 | 11.98 | 12.25 | 21.22 | 21.96 |
| Vector | 200.51 | 122.71 | 134.42 | 134.37 | 118.84 | 120.72 |
| PMR Vector | 135.56 | 139.58 | 146.14 | 143.74 | 128.09 | 129.40 |
| OwnedBlock | 13.46 | 12.11 | 10.44 | 9.51 | 17.82 | 17.57 |

42/42 cases passed.

### macOS · AppleClang 17.0.0.17000013

| Scenario | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Allocate/free 16 B | 17.72 | 21.13 FAIL | 5.18 | 5.74 FAIL | 15.75 | 16.24 |
| Allocate/free 64 B | 22.92 | 26.21 FAIL | 5.43 | 5.99 | 17.31 | 17.76 |
| Allocate/free 256 B | 22.27 | 23.36 | 7.03 | 7.65 | 17.98 | 19.67 |
| Allocate/free 4 KiB | 27.86 | 27.34 | 15.95 | 15.13 | 24.00 | 24.83 |
| Allocate/free 64 KiB | 29.30 | 27.23 | 16.64 | 16.69 | 207.79 | 208.90 |
| Zeroed allocation | 25.45 | 27.42 | 7.27 | 8.73 FAIL | 21.63 | 25.80 FAIL |
| Reallocate | 57.81 | 62.08 | 14.18 | 15.84 FAIL | 49.30 | 48.07 |
| Reallocate with zeroed growth | 56.49 | 61.08 | 16.21 | 16.38 | 49.39 | 53.11 |
| Unique object | 29.75 | 34.80 FAIL | 6.21 | 7.52 FAIL | 17.08 | 19.57 FAIL |
| Object array | 69.44 | 80.61 FAIL | 21.11 | 29.88 FAIL | 36.83 | 49.20 FAIL |
| Shared object | 33.71 | 32.67 | 21.16 | 20.99 | 35.10 | 33.19 |
| Vector | 145.62 | 161.06 FAIL | 737.06 | 134.92 | 824.46 | 177.12 |
| PMR Vector | 160.80 | 158.84 | 142.00 | 154.77 | 172.47 | 180.88 |
| OwnedBlock | 20.78 | 21.99 | 5.31 | 5.70 | 19.34 | 21.82 FAIL |

28/42 cases passed.

Data is retained in `docs/results/latest-validation.json`; the overview figure uses `docs/results/latest-overview.json`. Some Linux/macOS object and container results differ by several times; allocator-only conclusions require review of the benchmark and generated code. Historical charts and applications retain their separate provenance.

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
