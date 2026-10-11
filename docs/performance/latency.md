# Allocation time

[Performance](../performance.md) · **English** · [简体中文](latency.zh-CN.md)

**Nanoseconds per complete operation; lower is faster.** Global, counters off, Release builds. Trial count and aggregation are recorded below.

<!-- performance-source -->
2026-10-11 · [8365472](https://github.com/dugan-dev/UniMemory/actions/runs/38167739252) · [CSV](../results/current/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

## Object, container and block

### Windows

<!-- performance-table:workloads-windows-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 35.1 [34.0, 36.4], CV 2.2% | 6.3 [6.1, 6.5], CV 2.2% | 28.6 [27.6, 29.7], CV 2.4% |
| make_shared, 64 B | 50.6 [48.6, 52.4], CV 2.2% | 19.2 [18.3, 19.5], CV 2.2% | 45.6 [44.7, 47.1], CV 1.8% |
| vector, 32 uint64_t | 432.7 [408.7, 460.1], CV 3.7% | 168.8 [160.8, 182.9], CV 3.6% | 389.7 [366.0, 466.1], CV 7.5% |
| PMR vector, 32 uint64_t | 460.0 [441.5, 477.2], CV 2.4% | 187.8 [183.5, 192.8], CV 1.7% | 390.8 [371.7, 424.1], CV 4.4% |
| Zeroed Block, 4 KiB | 67.6 [64.7, 70.5], CV 2.5% | 45.3 [41.7, 68.7], CV 18.3% | 54.5 [52.0, 57.1], CV 2.9% |
| Block resize, 4 to 8 KiB | 143.1 [137.9, 148.4], CV 2.4% | 80.9 [74.9, 107.9], CV 12.0% | 120.5 [116.4, 127.6], CV 3.1% |
| Mixed lifetimes, 4-16 KiB | 47.7 [46.0, 50.5], CV 2.8% | 29.2 [28.4, 29.8], CV 1.7% | 102.8 [98.2, 115.7], CV 4.8% |
| Cross-thread free, 64 B, 8 threads | 81.5 [72.9, 116.4], CV 15.5% | 42.3 [36.7, 48.4], CV 9.6% | 70.8 [63.6, 78.0], CV 6.8% |
<!-- /performance-table -->

### Linux

<!-- performance-table:workloads-linux-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 10.7 [10.6, 10.9], CV 1.3% | 5.9 [5.8, 6.1], CV 1.5% | 10.8 [10.7, 11.2], CV 1.5% |
| make_shared, 64 B | 12.9 [12.6, 13.2], CV 1.2% | 8.5 [8.3, 8.9], CV 2.2% | 16.9 [16.7, 17.4], CV 1.3% |
| vector, 32 uint64_t | 95.3 [92.5, 99.0], CV 2.1% | 63.0 [62.2, 64.6], CV 1.3% | 112.6 [110.8, 118.1], CV 2.5% |
| PMR vector, 32 uint64_t | 93.5 [91.4, 97.3], CV 1.8% | 72.4 [71.0, 75.9], CV 1.9% | 122.4 [121.6, 125.5], CV 1.1% |
| Zeroed Block, 4 KiB | 74.5 [69.6, 78.4], CV 3.3% | 54.8 [54.2, 57.3], CV 1.9% | 59.3 [58.9, 61.0], CV 1.1% |
| Block resize, 4 to 8 KiB | 124.8 [103.2, 174.5], CV 19.2% | 80.3 [77.8, 96.2], CV 7.1% | 93.7 [93.2, 94.7], CV 0.6% |
| Mixed lifetimes, 4-16 KiB | 53.4 [49.5, 62.1], CV 7.8% | 21.6 [21.5, 21.8], CV 0.4% | 56.0 [53.8, 59.1], CV 2.9% |
| Cross-thread free, 64 B, 8 threads | 43.6 [36.7, 50.7], CV 12.1% | 21.2 [19.1, 22.6], CV 4.9% | 40.8 [31.8, 51.0], CV 15.1% |
<!-- /performance-table -->

### macOS

<!-- performance-table:workloads-macos-arm64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 19.6 [18.0, 22.0], CV 7.2% | 5.5 [4.9, 6.5], CV 10.6% | 15.5 [13.7, 19.4], CV 12.5% |
| make_shared, 64 B | 20.5 [19.0, 24.9], CV 8.6% | 11.1 [10.3, 12.4], CV 6.3% | 19.9 [17.8, 25.2], CV 11.8% |
| vector, 32 uint64_t | 153.2 [133.8, 209.7], CV 16.5% | 79.0 [69.8, 100.0], CV 13.0% | 142.9 [127.8, 212.0], CV 18.0% |
| PMR vector, 32 uint64_t | 152.9 [141.2, 182.8], CV 9.5% | 94.3 [86.0, 116.9], CV 10.3% | 158.9 [149.2, 196.1], CV 9.1% |
| Zeroed Block, 4 KiB | 95.8 [83.7, 116.7], CV 10.8% | 64.3 [59.8, 83.0], CV 10.6% | 71.5 [65.9, 85.9], CV 8.9% |
| Block resize, 4 to 8 KiB | 140.5 [123.6, 160.9], CV 9.6% | 105.2 [94.8, 124.8], CV 9.3% | 129.4 [115.5, 168.8], CV 13.2% |
| Mixed lifetimes, 4-16 KiB | 33.5 [30.4, 40.4], CV 9.6% | 22.1 [20.7, 24.7], CV 5.7% | 23.2 [21.6, 26.5], CV 7.7% |
| Cross-thread free, 64 B, 8 threads | 12769.5 [150.4, 23916.4], CV 65.6% | 224.7 [142.6, 471.9], CV 44.1% | 9398.2 [199.4, 22170.6], CV 76.0% |
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
| 16 | Standard | 37.2 [35.5, 39.1], CV 3.3% | 37.4 [36.2, 39.7], CV 2.7% | 59.0 [56.6, 63.7], CV 3.3% |
| 16 | mimalloc | 6.0 [5.7, 6.1], CV 2.1% | 7.0 [6.8, 7.2], CV 1.7% | 27.2 [26.5, 27.7], CV 1.3% |
| 16 | jemalloc | 29.8 [29.3, 30.6], CV 1.3% | 30.3 [29.3, 32.0], CV 3.0% | 52.7 [50.8, 54.4], CV 2.0% |
| 64 | Standard | 70.6 [34.9, 372.9], CV 150.4% | 71.1 [35.8, 376.3], CV 151.0% | 59.3 [57.2, 61.1], CV 1.8% |
| 64 | mimalloc | 6.3 [6.1, 6.6], CV 2.6% | 7.3 [7.1, 7.4], CV 1.3% | 27.4 [26.5, 27.7], CV 1.2% |
| 64 | jemalloc | 29.6 [28.8, 30.5], CV 1.7% | 30.1 [29.5, 30.9], CV 1.5% | 52.2 [51.2, 53.5], CV 1.3% |
| 256 | Standard | 37.5 [35.6, 42.0], CV 4.8% | 37.6 [36.3, 38.9], CV 2.3% | 59.1 [57.0, 60.5], CV 1.9% |
| 256 | mimalloc | 7.4 [7.3, 7.7], CV 2.1% | 8.1 [7.9, 8.3], CV 2.0% | 28.2 [27.6, 28.9], CV 1.6% |
| 256 | jemalloc | 30.0 [29.3, 31.0], CV 1.8% | 30.1 [29.1, 32.0], CV 2.6% | 52.3 [51.1, 53.9], CV 2.1% |
| 4096 | Standard | 37.1 [35.1, 38.5], CV 2.8% | 38.2 [36.4, 41.2], CV 4.0% | 58.5 [57.1, 59.7], CV 1.6% |
| 4096 | mimalloc | 25.4 [24.6, 26.0], CV 1.9% | 25.9 [25.0, 26.8], CV 2.0% | 45.0 [43.6, 46.2], CV 1.7% |
| 4096 | jemalloc | 32.3 [31.5, 33.4], CV 1.7% | 32.8 [32.0, 34.6], CV 2.3% | 55.5 [52.8, 59.2], CV 3.3% |
| 65536 | Standard | 113.1 [109.1, 126.3], CV 4.5% | 113.9 [109.0, 118.6], CV 2.2% | 132.1 [129.6, 134.2], CV 1.1% |
| 65536 | mimalloc | 26.7 [25.2, 28.1], CV 3.7% | 27.5 [26.6, 29.0], CV 2.4% | 47.1 [46.0, 48.8], CV 1.9% |
| 65536 | jemalloc | 258.5 [252.3, 263.1], CV 1.5% | 258.9 [249.4, 266.8], CV 2.4% | 268.1 [261.9, 277.6], CV 2.3% |
<!-- /performance-table -->

### Linux

<!-- performance-table:latency-linux-x64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 13.4 [13.4, 13.5], CV 0.2% | 13.8 [13.6, 14.6], CV 2.2% | 14.2 [14.2, 14.3], CV 0.2% |
| 16 | mimalloc | 7.0 [6.9, 7.2], CV 1.6% | 7.3 [7.2, 7.5], CV 1.7% | 9.2 [8.8, 11.6], CV 9.5% |
| 16 | jemalloc | 14.9 [14.7, 16.3], CV 3.4% | 15.3 [15.1, 16.5], CV 3.0% | 16.8 [16.4, 18.8], CV 4.7% |
| 64 | Standard | 13.4 [12.9, 13.7], CV 1.5% | 13.7 [13.6, 13.7], CV 0.2% | 14.8 [14.2, 19.7], CV 11.8% |
| 64 | mimalloc | 7.2 [7.0, 7.6], CV 2.6% | 7.3 [7.2, 7.6], CV 1.4% | 8.9 [8.8, 9.0], CV 0.4% |
| 64 | jemalloc | 14.9 [14.7, 15.3], CV 1.1% | 15.4 [15.0, 16.7], CV 4.1% | 17.9 [16.5, 27.7], CV 19.5% |
| 256 | Standard | 13.4 [13.4, 13.6], CV 0.6% | 13.7 [13.6, 13.7], CV 0.2% | 14.2 [14.2, 14.6], CV 0.9% |
| 256 | mimalloc | 7.8 [7.7, 8.1], CV 1.9% | 8.0 [7.7, 8.2], CV 2.0% | 9.1 [8.9, 9.6], CV 2.3% |
| 256 | jemalloc | 15.0 [14.8, 15.5], CV 1.9% | 15.3 [15.1, 15.8], CV 1.7% | 17.0 [16.6, 17.9], CV 2.8% |
| 4096 | Standard | 22.1 [22.1, 22.2], CV 0.2% | 22.4 [22.4, 22.5], CV 0.2% | 23.6 [23.5, 24.4], CV 1.2% |
| 4096 | mimalloc | 18.1 [17.0, 19.2], CV 3.6% | 18.5 [17.4, 19.2], CV 2.5% | 19.5 [18.7, 20.0], CV 2.1% |
| 4096 | jemalloc | 16.9 [16.7, 17.8], CV 1.9% | 17.2 [17.1, 17.9], CV 1.4% | 18.9 [18.7, 19.3], CV 1.1% |
| 65536 | Standard | 24.3 [24.3, 24.4], CV 0.1% | 24.6 [24.6, 24.6], CV 0.1% | 25.7 [25.7, 25.8], CV 0.2% |
| 65536 | mimalloc | 18.8 [17.9, 19.4], CV 2.4% | 19.0 [18.4, 20.1], CV 3.0% | 19.8 [19.3, 20.3], CV 1.8% |
| 65536 | jemalloc | 235.8 [201.1, 249.3], CV 6.8% | 239.9 [198.9, 250.1], CV 6.8% | 243.3 [200.1, 250.8], CV 6.5% |
<!-- /performance-table -->

### macOS

<!-- performance-table:latency-macos-arm64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 16.7 [15.7, 18.1], CV 5.3% | 17.1 [16.5, 19.2], CV 5.1% | 17.9 [17.0, 19.3], CV 4.8% |
| 16 | mimalloc | 5.4 [4.7, 7.4], CV 19.2% | 5.8 [5.0, 8.5], CV 18.9% | 6.3 [6.0, 6.7], CV 3.2% |
| 16 | jemalloc | 15.6 [14.6, 18.7], CV 8.0% | 15.6 [14.9, 16.9], CV 3.8% | 17.1 [16.0, 19.6], CV 6.8% |
| 64 | Standard | 20.1 [18.7, 23.5], CV 7.9% | 20.4 [18.7, 25.3], CV 9.7% | 21.5 [19.7, 25.2], CV 8.7% |
| 64 | mimalloc | 5.7 [5.0, 7.3], CV 14.4% | 5.8 [5.3, 7.4], CV 11.3% | 6.4 [6.1, 7.4], CV 7.3% |
| 64 | jemalloc | 16.9 [14.7, 27.4], CV 23.0% | 16.4 [15.1, 21.1], CV 11.2% | 17.0 [16.1, 17.8], CV 3.0% |
| 256 | Standard | 18.6 [18.1, 19.3], CV 2.4% | 18.7 [17.8, 19.8], CV 2.9% | 20.3 [18.8, 26.5], CV 11.4% |
| 256 | mimalloc | 6.4 [5.9, 7.1], CV 6.9% | 7.1 [6.3, 9.8], CV 16.1% | 7.3 [6.9, 8.2], CV 6.2% |
| 256 | jemalloc | 16.0 [14.8, 19.6], CV 8.6% | 16.8 [15.1, 25.3], CV 17.9% | 17.4 [16.6, 19.7], CV 6.6% |
| 4096 | Standard | 22.2 [20.8, 24.1], CV 5.4% | 22.4 [21.7, 23.7], CV 2.7% | 24.9 [22.0, 34.1], CV 14.7% |
| 4096 | mimalloc | 16.0 [13.6, 26.1], CV 23.6% | 15.9 [14.3, 24.1], CV 19.3% | 16.3 [15.6, 17.9], CV 5.2% |
| 4096 | jemalloc | 19.8 [19.3, 20.5], CV 2.1% | 20.1 [19.1, 22.7], CV 5.3% | 22.8 [20.7, 31.2], CV 14.8% |
| 65536 | Standard | 24.2 [22.7, 27.4], CV 5.4% | 24.7 [23.0, 27.3], CV 6.4% | 26.6 [24.1, 34.0], CV 11.6% |
| 65536 | mimalloc | 17.3 [15.7, 22.9], CV 13.0% | 18.0 [15.4, 23.3], CV 17.1% | 17.8 [17.1, 19.7], CV 5.4% |
| 65536 | jemalloc | 172.5 [160.9, 227.4], CV 11.9% | 177.2 [159.3, 215.4], CV 12.2% | 171.5 [159.6, 191.7], CV 6.3% |
<!-- /performance-table -->

Small differences may be measurement noise; counter cost varies by backend and size. Successful requests match in size and alignment.

## Cross-thread free

One thread allocates 64 B blocks; workers free them. Worker creation is outside timing; completion is inside.

<!-- performance-table:cross-thread -->
| Platform | Workers | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 83.8 [80.1, 88.7], CV 3.1% | 54.0 [51.6, 57.3], CV 3.3% | 86.4 [83.5, 89.2], CV 1.8% |
| Windows | 4 | 73.7 [65.3, 78.1], CV 5.3% | 41.3 [28.8, 43.7], CV 10.8% | 71.6 [62.8, 74.6], CV 4.6% |
| Windows | 8 | 81.5 [72.9, 116.4], CV 15.5% | 42.3 [36.7, 48.4], CV 9.6% | 70.8 [63.6, 78.0], CV 6.8% |
| Linux | 2 | 52.2 [48.1, 54.6], CV 4.4% | 19.2 [18.8, 19.7], CV 1.5% | 46.8 [45.0, 49.1], CV 2.6% |
| Linux | 4 | 46.1 [41.3, 48.4], CV 5.1% | 20.5 [18.6, 21.5], CV 4.4% | 45.0 [39.8, 50.6], CV 9.0% |
| Linux | 8 | 43.6 [36.7, 50.7], CV 12.1% | 21.2 [19.1, 22.6], CV 4.9% | 40.8 [31.8, 51.0], CV 15.1% |
| macOS | 2 | 4073.0 [511.3, 12329.7], CV 89.1% | 3736.1 [84.3, 11151.2], CV 110.7% | 4357.0 [1563.3, 7694.3], CV 46.8% |
| macOS | 4 | 6103.7 [465.0, 19877.7], CV 118.8% | 5542.6 [329.2, 16369.4], CV 111.7% | 5681.0 [211.9, 13512.7], CV 100.7% |
| macOS | 8 | 12769.5 [150.4, 23916.4], CV 65.6% | 224.7 [142.6, 471.9], CV 44.1% | 9398.2 [199.4, 22170.6], CV 76.0% |
<!-- /performance-table -->

## Stack

Prepared Buffer, one mark/allocate/rewind cycle, alignment 16. Writes one byte; its work differs from Heap allocation.

<!-- performance-table:stack -->
| Platform | Bytes | ns/cycle |
| --- | --- | --- |
| Windows | 64 | 2.98 [2.86, 3.04], CV 1.8% |
| Windows | 4096 | 3.00 [2.89, 3.12], CV 2.1% |
| Linux | 64 | 4.05 [4.05, 4.06], CV 0.1% |
| Linux | 4096 | 4.05 [4.05, 4.06], CV 0.1% |
| macOS | 64 | 4.20 [3.81, 4.73], CV 7.9% |
| macOS | 4096 | 4.18 [3.86, 4.73], CV 7.1% |
<!-- /performance-table -->

CPU affinity is unpinned. These are complete-operation medians; individual-allocation p95/p99 are on the performance overview.

[Method](../benchmarking.md) · [Raw data](../results/current/README.md) · [Memory](memory.md)
