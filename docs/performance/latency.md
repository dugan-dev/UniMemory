# Allocation time

[Performance](../performance.md) · **English** · [简体中文](latency.zh-CN.md)

**Nanoseconds per complete operation; lower is faster.** Global, counters off, Release builds. Trial count and aggregation are recorded below.

<!-- performance-source -->
2026-10-11 · [5953761](https://github.com/dugan-dev/UniMemory/actions/runs/38122935359) · [CSV](../results/current/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

## Object, container and block

### Windows

<!-- performance-table:workloads-windows-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 33.6 [33.3, 34.5], CV 1.1% | 5.2 [4.5, 6.9], CV 18.4% | 32.3 [30.0, 42.0], CV 10.8% |
| make_shared, 64 B | 73.3 [37.6, 210.1], CV 98.3% | 9.2 [9.0, 9.6], CV 2.1% | 36.3 [35.6, 37.3], CV 1.7% |
| vector, 32 uint64_t | 415.1 [409.4, 424.8], CV 1.2% | 135.0 [128.7, 147.1], CV 3.9% | 394.5 [383.7, 449.9], CV 5.0% |
| PMR vector, 32 uint64_t | 432.3 [428.5, 442.8], CV 1.1% | 144.5 [142.1, 147.0], CV 1.3% | 401.3 [387.6, 484.5], CV 7.3% |
| Zeroed Block, 4 KiB | 71.3 [70.4, 71.9], CV 0.6% | 49.2 [48.2, 51.6], CV 2.6% | 68.9 [67.2, 79.0], CV 5.3% |
| Block resize, 4 to 8 KiB | 127.4 [123.7, 133.4], CV 2.3% | 89.0 [75.1, 165.1], CV 31.2% | 131.3 [129.7, 135.1], CV 1.6% |
| Mixed lifetimes, 4-16 KiB | 41.1 [39.6, 42.7], CV 2.8% | 23.5 [23.3, 24.2], CV 1.1% | 89.3 [88.7, 91.0], CV 0.8% |
| Cross-thread free, 64 B, 8 threads | 57.8 [52.0, 91.5], CV 20.7% | 27.2 [25.5, 29.8], CV 4.9% | 59.4 [57.1, 61.1], CV 2.0% |
<!-- /performance-table -->

### Linux

<!-- performance-table:workloads-linux-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 12.8 [12.8, 13.2], CV 1.0% | 7.2 [7.1, 7.5], CV 2.0% | 13.9 [13.8, 14.4], CV 1.5% |
| make_shared, 64 B | 15.6 [15.6, 16.0], CV 0.9% | 11.7 [11.1, 12.3], CV 3.2% | 21.5 [21.4, 22.1], CV 1.2% |
| vector, 32 uint64_t | 124.8 [124.1, 128.0], CV 0.9% | 79.9 [79.5, 80.5], CV 0.4% | 150.0 [148.6, 156.1], CV 1.6% |
| PMR vector, 32 uint64_t | 116.9 [116.1, 119.7], CV 0.9% | 90.8 [89.1, 95.2], CV 2.0% | 162.8 [162.0, 165.7], CV 0.8% |
| Zeroed Block, 4 KiB | 116.7 [105.9, 122.7], CV 5.6% | 89.0 [88.4, 90.7], CV 0.7% | 89.3 [89.0, 90.4], CV 0.5% |
| Block resize, 4 to 8 KiB | 471.1 [137.8, 1795.0], CV 146.8% | 106.1 [102.8, 128.2], CV 7.4% | 125.7 [124.9, 126.6], CV 0.4% |
| Mixed lifetimes, 4-16 KiB | 64.1 [58.9, 74.6], CV 8.4% | 27.5 [27.3, 27.8], CV 0.7% | 73.1 [69.6, 75.7], CV 3.0% |
| Cross-thread free, 64 B, 8 threads | 58.0 [32.9, 72.8], CV 28.4% | 25.9 [24.1, 27.6], CV 4.2% | 43.9 [41.1, 46.1], CV 4.0% |
<!-- /performance-table -->

### macOS

<!-- performance-table:workloads-macos-arm64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 19.9 [17.4, 23.3], CV 10.1% | 5.1 [4.8, 5.5], CV 3.5% | 15.2 [13.4, 17.3], CV 8.7% |
| make_shared, 64 B | 21.3 [17.8, 27.1], CV 15.4% | 10.5 [9.7, 11.7], CV 6.9% | 19.9 [17.2, 23.1], CV 8.8% |
| vector, 32 uint64_t | 151.4 [130.9, 217.8], CV 17.1% | 81.3 [76.3, 85.3], CV 4.3% | 142.0 [123.7, 184.0], CV 12.4% |
| PMR vector, 32 uint64_t | 148.7 [134.6, 167.3], CV 6.8% | 92.8 [83.1, 103.2], CV 5.8% | 160.5 [151.1, 171.2], CV 5.2% |
| Zeroed Block, 4 KiB | 72.7 [60.0, 94.6], CV 13.8% | 62.0 [55.7, 71.4], CV 9.3% | 70.5 [63.0, 81.1], CV 8.5% |
| Block resize, 4 to 8 KiB | 143.8 [123.4, 179.5], CV 11.9% | 98.8 [91.3, 121.8], CV 9.2% | 127.3 [113.6, 141.1], CV 7.4% |
| Mixed lifetimes, 4-16 KiB | 32.1 [29.3, 39.9], CV 9.4% | 21.3 [19.8, 22.5], CV 3.9% | 22.5 [20.9, 24.1], CV 4.4% |
| Cross-thread free, 64 B, 8 threads | 5091.6 [200.0, 21434.7], CV 139.0% | 2279.3 [153.6, 21220.9], CV 292.0% | 4903.1 [180.9, 21241.8], CV 158.2% |
<!-- /performance-table -->

Objects include construction/destruction; vectors grow to 32 uint64_t values; resize includes allocation, growth and release. Compare within each row.

![Object and block allocation time](../images/workload-comparison.png)

![macOS object and block allocation time](../images/performance/macos-arm64/workloads.svg)

Standard = 1; shorter is faster. The macOS chart covers selected API scenarios; all eight workloads are in the tables.

## Native interface and counters

One allocation/free pair, alignment 16. Native calls the backend directly; API uses UniMemory with counters off; Basic enables counters. Native/API share the OFF build; Basic comes from the ON build.

### Windows

<!-- performance-table:latency-windows-x64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 35.1 [35.0, 35.3], CV 0.3% | 35.6 [35.3, 36.8], CV 1.3% | 37.0 [36.5, 39.5], CV 2.6% |
| 16 | mimalloc | 5.1 [5.0, 5.3], CV 2.0% | 5.3 [5.2, 5.6], CV 1.8% | 8.9 [8.9, 9.0], CV 0.2% |
| 16 | jemalloc | 38.9 [34.6, 42.2], CV 6.3% | 38.7 [33.1, 46.9], CV 9.4% | 39.3 [35.1, 43.4], CV 7.2% |
| 64 | Standard | 35.5 [35.0, 37.0], CV 2.1% | 35.9 [35.3, 38.1], CV 2.4% | 36.9 [36.5, 37.8], CV 1.1% |
| 64 | mimalloc | 5.1 [5.1, 5.2], CV 0.3% | 5.4 [5.4, 5.5], CV 0.5% | 9.0 [8.9, 9.1], CV 0.6% |
| 64 | jemalloc | 40.4 [37.1, 48.9], CV 8.5% | 38.1 [36.0, 41.3], CV 4.3% | 38.3 [35.7, 43.8], CV 6.7% |
| 256 | Standard | 36.0 [35.7, 37.7], CV 1.7% | 36.4 [36.2, 37.1], CV 0.9% | 37.8 [37.2, 38.9], CV 1.8% |
| 256 | mimalloc | 5.8 [5.8, 5.8], CV 0.3% | 6.1 [6.1, 6.1], CV 0.3% | 9.2 [9.1, 9.3], CV 0.5% |
| 256 | jemalloc | 38.9 [31.6, 42.4], CV 8.9% | 38.8 [33.7, 41.0], CV 6.2% | 37.7 [35.7, 40.7], CV 4.4% |
| 4096 | Standard | 36.1 [35.7, 37.3], CV 1.3% | 36.8 [36.2, 39.4], CV 2.6% | 38.2 [37.5, 39.9], CV 2.3% |
| 4096 | mimalloc | 20.6 [20.2, 20.9], CV 1.0% | 20.7 [20.5, 21.2], CV 1.2% | 21.7 [21.5, 21.8], CV 0.4% |
| 4096 | jemalloc | 39.6 [34.7, 43.0], CV 6.4% | 39.7 [37.6, 41.9], CV 3.6% | 38.0 [37.2, 40.1], CV 2.7% |
| 65536 | Standard | 94.7 [92.0, 108.4], CV 5.2% | 94.3 [92.8, 101.4], CV 2.7% | 95.1 [93.7, 97.5], CV 1.4% |
| 65536 | mimalloc | 23.1 [22.4, 27.2], CV 6.3% | 23.0 [22.7, 23.2], CV 0.7% | 24.2 [23.5, 30.0], CV 8.4% |
| 65536 | jemalloc | 218.5 [216.4, 229.8], CV 1.8% | 218.5 [216.6, 230.0], CV 1.9% | 219.2 [217.6, 224.1], CV 0.9% |
<!-- /performance-table -->

### Linux

<!-- performance-table:latency-linux-x64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 17.1 [17.1, 17.2], CV 0.2% | 18.0 [17.8, 18.2], CV 0.9% | 21.4 [18.7, 46.1], CV 40.4% |
| 16 | mimalloc | 8.5 [8.2, 8.7], CV 2.0% | 9.2 [8.9, 9.9], CV 3.0% | 10.1 [10.0, 10.4], CV 1.2% |
| 16 | jemalloc | 19.2 [19.2, 19.3], CV 0.2% | 19.9 [19.8, 20.1], CV 0.6% | 22.6 [22.0, 23.9], CV 2.9% |
| 64 | Standard | 17.2 [17.1, 17.7], CV 1.0% | 18.1 [17.8, 18.7], CV 1.4% | 18.7 [18.7, 18.9], CV 0.4% |
| 64 | mimalloc | 8.8 [8.6, 8.9], CV 1.2% | 9.3 [9.2, 9.5], CV 1.7% | 10.2 [10.1, 10.5], CV 1.5% |
| 64 | jemalloc | 19.5 [19.1, 20.9], CV 3.1% | 20.2 [19.8, 20.7], CV 1.6% | 22.5 [22.1, 23.5], CV 1.8% |
| 256 | Standard | 17.2 [17.1, 17.2], CV 0.2% | 18.0 [17.8, 18.6], CV 1.4% | 18.7 [18.7, 19.0], CV 0.5% |
| 256 | mimalloc | 9.3 [9.1, 9.8], CV 2.3% | 10.0 [9.7, 10.4], CV 2.6% | 10.8 [10.7, 11.0], CV 1.2% |
| 256 | jemalloc | 19.5 [19.0, 20.2], CV 1.8% | 19.9 [19.7, 20.4], CV 1.0% | 22.5 [22.2, 23.8], CV 2.1% |
| 4096 | Standard | 29.9 [29.9, 29.9], CV 0.1% | 30.2 [30.2, 30.3], CV 0.1% | 31.5 [31.5, 31.5], CV 0.1% |
| 4096 | mimalloc | 24.4 [23.8, 26.9], CV 4.1% | 24.3 [24.0, 25.2], CV 1.7% | 25.3 [24.5, 29.6], CV 6.0% |
| 4096 | jemalloc | 22.5 [22.3, 23.4], CV 1.9% | 23.2 [22.8, 24.5], CV 2.5% | 25.5 [25.2, 26.4], CV 1.7% |
| 65536 | Standard | 32.7 [32.7, 32.8], CV 0.1% | 33.1 [33.0, 33.4], CV 0.4% | 34.5 [34.2, 35.8], CV 1.4% |
| 65536 | mimalloc | 25.3 [25.0, 26.1], CV 1.4% | 25.5 [25.4, 25.8], CV 0.5% | 27.5 [26.8, 29.5], CV 3.2% |
| 65536 | jemalloc | 354.0 [298.7, 370.5], CV 6.6% | 353.5 [286.6, 373.1], CV 7.8% | 364.7 [341.7, 368.9], CV 2.2% |
<!-- /performance-table -->

### macOS

<!-- performance-table:latency-macos-arm64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 16.1 [15.0, 16.8], CV 4.1% | 16.6 [15.6, 18.0], CV 4.4% | 19.3 [16.4, 34.1], CV 27.5% |
| 16 | mimalloc | 5.0 [4.5, 5.6], CV 6.9% | 5.3 [4.8, 5.9], CV 6.0% | 6.2 [5.8, 8.0], CV 10.3% |
| 16 | jemalloc | 17.0 [14.4, 22.0], CV 16.6% | 17.5 [15.1, 22.0], CV 15.8% | 17.9 [15.5, 19.6], CV 7.1% |
| 64 | Standard | 19.4 [17.6, 22.7], CV 8.3% | 19.4 [18.4, 21.6], CV 5.1% | 21.3 [19.2, 27.1], CV 11.0% |
| 64 | mimalloc | 5.3 [4.7, 6.0], CV 7.8% | 5.6 [5.1, 6.2], CV 6.8% | 6.2 [5.8, 7.3], CV 6.9% |
| 64 | jemalloc | 16.8 [14.3, 18.3], CV 7.8% | 17.0 [14.9, 24.5], CV 17.3% | 17.6 [16.0, 19.1], CV 4.8% |
| 256 | Standard | 18.5 [17.3, 21.6], CV 6.8% | 18.7 [17.2, 21.4], CV 7.3% | 19.9 [18.0, 22.9], CV 7.1% |
| 256 | mimalloc | 6.4 [5.7, 8.5], CV 13.3% | 6.7 [6.0, 8.3], CV 10.5% | 7.4 [6.7, 10.2], CV 14.1% |
| 256 | jemalloc | 16.5 [14.4, 19.3], CV 9.4% | 16.5 [14.5, 19.3], CV 9.9% | 18.3 [15.8, 25.7], CV 15.3% |
| 4096 | Standard | 22.3 [20.1, 24.8], CV 5.7% | 23.4 [22.0, 25.6], CV 4.5% | 24.4 [21.1, 32.1], CV 13.3% |
| 4096 | mimalloc | 14.7 [13.3, 17.6], CV 8.4% | 14.6 [13.3, 16.1], CV 6.4% | 16.5 [14.6, 26.3], CV 21.2% |
| 4096 | jemalloc | 21.1 [19.2, 23.7], CV 8.1% | 21.3 [18.9, 26.4], CV 10.2% | 24.0 [19.9, 31.9], CV 17.9% |
| 65536 | Standard | 24.0 [22.2, 27.4], CV 6.7% | 24.9 [22.4, 28.2], CV 8.4% | 25.9 [23.4, 30.5], CV 9.0% |
| 65536 | mimalloc | 16.6 [14.9, 18.7], CV 7.0% | 16.6 [14.8, 22.4], CV 13.3% | 17.6 [16.1, 19.9], CV 7.3% |
| 65536 | jemalloc | 173.8 [153.3, 204.7], CV 8.8% | 174.8 [154.5, 203.0], CV 7.2% | 182.3 [154.3, 234.4], CV 12.4% |
<!-- /performance-table -->

Small differences may be measurement noise; counter cost varies by backend and size. Successful requests match in size and alignment.

## Cross-thread free

One thread allocates 64 B blocks; workers free them. Worker creation is outside timing; completion is inside.

<!-- performance-table:cross-thread -->
| Platform | Workers | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 49.4 [48.3, 51.4], CV 2.0% | 22.5 [21.7, 23.3], CV 2.4% | 65.6 [61.2, 67.8], CV 2.9% |
| Windows | 4 | 51.4 [50.2, 52.4], CV 1.4% | 23.2 [19.2, 24.9], CV 8.0% | 54.2 [52.4, 57.9], CV 2.9% |
| Windows | 8 | 57.8 [52.0, 91.5], CV 20.7% | 27.2 [25.5, 29.8], CV 4.9% | 59.4 [57.1, 61.1], CV 2.0% |
| Linux | 2 | 67.5 [61.7, 72.1], CV 5.4% | 24.7 [23.4, 25.5], CV 2.6% | 42.1 [40.8, 43.8], CV 2.1% |
| Linux | 4 | 54.9 [30.5, 67.9], CV 23.2% | 26.4 [23.1, 30.9], CV 7.8% | 43.9 [41.0, 46.0], CV 3.4% |
| Linux | 8 | 58.0 [32.9, 72.8], CV 28.4% | 25.9 [24.1, 27.6], CV 4.2% | 43.9 [41.1, 46.1], CV 4.0% |
| macOS | 2 | 6378.8 [701.9, 15531.9], CV 66.0% | 3243.1 [210.3, 8809.9], CV 91.1% | 2841.4 [674.7, 6732.6], CV 73.5% |
| macOS | 4 | 4694.9 [157.1, 15502.3], CV 133.1% | 4023.7 [462.7, 15813.4], CV 129.4% | 4472.8 [175.4, 18368.7], CV 125.7% |
| macOS | 8 | 5091.6 [200.0, 21434.7], CV 139.0% | 2279.3 [153.6, 21220.9], CV 292.0% | 4903.1 [180.9, 21241.8], CV 158.2% |
<!-- /performance-table -->

## Stack

Prepared Buffer, one mark/allocate/rewind cycle, alignment 16. Writes one byte; its work differs from Heap allocation.

<!-- performance-table:stack -->
| Platform | Bytes | ns/cycle |
| --- | --- | --- |
| Windows | 64 | 3.13 [2.99, 3.26], CV 3.4% |
| Windows | 4096 | 3.27 [3.07, 4.01], CV 8.3% |
| Linux | 64 | 5.50 [5.50, 5.52], CV 0.1% |
| Linux | 4096 | 5.50 [5.50, 5.50], CV 0.0% |
| macOS | 64 | 4.19 [3.79, 5.11], CV 8.5% |
| macOS | 4096 | 4.12 [3.70, 4.82], CV 8.5% |
<!-- /performance-table -->

CPU affinity is unpinned. These are complete-operation medians; individual-allocation p95/p99 are on the performance overview.

[Method](../benchmarking.md) · [Raw data](../results/current/README.md) · [Memory](memory.md)
