# Allocation time

[Performance](../performance.md) · **English** · [简体中文](latency.zh-CN.md)

**Nanoseconds per complete operation; lower is faster.** Global, counters off, Release builds. Trial count and aggregation are recorded below.

<!-- performance-source -->
2026-10-13 · [c917e81](https://github.com/dugan-dev/UniMemory/actions/runs/38415666741) · [CSV](../results/current/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

## Object, container and block

### Windows

<!-- performance-table:workloads-windows-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 34.3 [33.6, 35.8], CV 2.2% | 5.4 [4.9, 6.8], CV 10.2% | 36.3 [30.8, 76.0], CV 38.5% |
| make_shared, 64 B | 39.4 [38.5, 41.5], CV 2.3% | 10.6 [10.2, 11.0], CV 3.2% | 50.4 [45.1, 67.5], CV 15.0% |
| vector, 32 uint64_t | 433.2 [428.0, 438.5], CV 1.0% | 124.1 [122.8, 125.6], CV 0.9% | 405.4 [363.3, 711.3], CV 26.6% |
| PMR vector, 32 uint64_t | 472.8 [468.0, 483.3], CV 1.0% | 187.4 [185.1, 191.1], CV 1.1% | 486.1 [446.0, 753.7], CV 19.4% |
| Zeroed Block, 4 KiB | 69.3 [67.9, 71.1], CV 1.7% | 49.4 [48.2, 52.8], CV 2.5% | 70.2 [67.1, 92.7], CV 11.3% |
| Block resize, 4 to 8 KiB | 123.2 [118.1, 131.5], CV 3.2% | 87.0 [76.0, 156.1], CV 29.1% | 134.8 [128.7, 168.2], CV 9.3% |
| Mixed lifetimes, 4-16 KiB | 25.7 [25.2, 26.5], CV 2.0% | 23.9 [23.9, 24.1], CV 0.3% | 91.0 [89.1, 98.6], CV 3.3% |
| Cross-thread free, 64 B, 8 threads | 53.8 [49.5, 79.9], CV 17.3% | 26.1 [24.1, 28.8], CV 5.9% | 58.4 [56.4, 59.8], CV 1.8% |
<!-- /performance-table -->

### Linux

<!-- performance-table:workloads-linux-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 12.8 [12.8, 13.4], CV 1.6% | 7.2 [7.1, 7.5], CV 2.3% | 13.9 [13.6, 14.7], CV 2.2% |
| make_shared, 64 B | 14.4 [14.3, 14.6], CV 0.7% | 10.5 [10.3, 11.0], CV 1.9% | 22.7 [22.6, 23.0], CV 0.6% |
| vector, 32 uint64_t | 113.3 [112.5, 116.2], CV 1.1% | 85.7 [85.3, 86.4], CV 0.4% | 158.9 [155.4, 168.3], CV 2.9% |
| PMR vector, 32 uint64_t | 117.1 [116.2, 119.7], CV 0.9% | 77.6 [76.6, 79.8], CV 1.2% | 150.5 [148.4, 157.6], CV 2.0% |
| Zeroed Block, 4 KiB | 111.5 [100.8, 120.6], CV 7.2% | 89.2 [89.0, 89.7], CV 0.2% | 90.8 [90.4, 92.4], CV 0.6% |
| Block resize, 4 to 8 KiB | 139.0 [125.3, 150.4], CV 6.3% | 105.4 [102.2, 123.9], CV 6.3% | 127.8 [124.2, 135.5], CV 2.5% |
| Mixed lifetimes, 4-16 KiB | 54.4 [45.5, 60.4], CV 10.8% | 27.4 [27.3, 27.6], CV 0.3% | 72.8 [69.0, 74.9], CV 2.6% |
| Cross-thread free, 64 B, 8 threads | 50.8 [29.0, 60.8], CV 19.4% | 26.1 [24.7, 27.8], CV 3.3% | 43.7 [42.5, 44.2], CV 1.5% |
<!-- /performance-table -->

### macOS

<!-- performance-table:workloads-macos-arm64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 22.6 [19.9, 25.3], CV 7.3% | 6.2 [5.4, 7.6], CV 13.0% | 17.3 [14.9, 21.3], CV 14.8% |
| make_shared, 64 B | 23.4 [19.9, 30.0], CV 14.1% | 11.9 [10.8, 13.0], CV 7.0% | 22.7 [19.6, 27.0], CV 13.9% |
| vector, 32 uint64_t | 155.2 [137.8, 183.4], CV 10.3% | 80.2 [73.8, 104.6], CV 11.6% | 147.0 [129.0, 166.6], CV 9.4% |
| PMR vector, 32 uint64_t | 156.0 [139.7, 168.8], CV 6.9% | 75.3 [64.7, 91.2], CV 9.8% | 149.0 [133.6, 174.6], CV 8.0% |
| Zeroed Block, 4 KiB | 81.9 [69.2, 93.5], CV 9.7% | 75.1 [59.9, 100.8], CV 18.5% | 79.2 [68.8, 102.0], CV 11.8% |
| Block resize, 4 to 8 KiB | 161.3 [128.7, 198.8], CV 14.0% | 110.3 [100.8, 127.3], CV 7.6% | 140.6 [124.4, 169.5], CV 10.4% |
| Mixed lifetimes, 4-16 KiB | 35.8 [31.5, 48.8], CV 14.6% | 24.3 [21.3, 28.1], CV 7.6% | 25.6 [22.8, 29.4], CV 8.5% |
| Cross-thread free, 64 B, 8 threads | 5677.8 [193.5, 15319.7], CV 116.4% | 9728.0 [159.1, 23073.9], CV 100.0% | 7476.4 [156.5, 25598.6], CV 125.8% |
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
| 16 | Standard | 33.2 [32.6, 36.2], CV 3.2% | 33.5 [33.0, 35.0], CV 2.0% | 106.7 [104.9, 111.1], CV 1.9% |
| 16 | mimalloc | 5.1 [5.0, 5.2], CV 1.9% | 5.2 [5.0, 5.4], CV 3.4% | 84.6 [82.9, 91.5], CV 2.9% |
| 16 | jemalloc | 38.6 [35.9, 44.0], CV 7.0% | 41.1 [38.8, 42.6], CV 3.1% | 105.8 [104.6, 110.7], CV 1.7% |
| 64 | Standard | 33.3 [32.6, 36.3], CV 3.4% | 33.1 [33.0, 33.5], CV 0.4% | 106.2 [105.0, 108.4], CV 1.3% |
| 64 | mimalloc | 5.1 [5.1, 5.1], CV 0.3% | 5.2 [5.1, 5.2], CV 0.3% | 116.0 [83.5, 242.3], CV 56.9% |
| 64 | jemalloc | 38.2 [34.7, 45.9], CV 9.7% | 41.4 [39.2, 43.6], CV 3.0% | 105.8 [104.7, 107.6], CV 0.9% |
| 256 | Standard | 33.2 [32.6, 36.1], CV 3.1% | 33.1 [33.0, 33.3], CV 0.3% | 106.8 [105.0, 112.1], CV 2.1% |
| 256 | mimalloc | 6.6 [5.8, 13.6], CV 37.6% | 6.0 [6.0, 6.0], CV 0.2% | 85.9 [84.6, 91.3], CV 2.3% |
| 256 | jemalloc | 37.5 [35.9, 40.6], CV 3.7% | 41.0 [38.3, 42.7], CV 3.5% | 106.4 [105.1, 108.6], CV 1.2% |
| 4096 | Standard | 33.6 [33.3, 34.6], CV 1.2% | 34.7 [33.7, 36.4], CV 3.2% | 107.6 [106.0, 110.8], CV 1.4% |
| 4096 | mimalloc | 20.2 [19.9, 20.4], CV 0.8% | 20.7 [20.4, 21.1], CV 1.1% | 92.3 [91.9, 92.7], CV 0.3% |
| 4096 | jemalloc | 39.1 [35.8, 44.5], CV 7.9% | 40.9 [38.4, 44.5], CV 4.2% | 112.1 [109.5, 123.5], CV 3.8% |
| 65536 | Standard | 91.2 [90.0, 95.3], CV 1.8% | 91.7 [90.4, 95.6], CV 1.8% | 167.8 [164.0, 175.5], CV 2.2% |
| 65536 | mimalloc | 22.5 [22.1, 24.1], CV 2.6% | 23.0 [22.6, 24.1], CV 1.7% | 115.0 [95.1, 271.5], CV 48.0% |
| 65536 | jemalloc | 222.2 [219.3, 233.6], CV 1.9% | 222.4 [219.6, 234.8], CV 2.0% | 358.0 [355.4, 365.0], CV 0.8% |
<!-- /performance-table -->

### Linux

<!-- performance-table:latency-linux-x64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 13.3 [13.1, 13.5], CV 1.0% | 13.3 [13.1, 13.5], CV 1.1% | 67.8 [67.3, 68.6], CV 0.6% |
| 16 | mimalloc | 8.1 [7.9, 8.2], CV 1.6% | 8.4 [8.2, 8.8], CV 2.4% | 61.3 [61.0, 62.4], CV 0.8% |
| 16 | jemalloc | 19.7 [19.4, 22.3], CV 4.7% | 20.0 [19.5, 22.3], CV 4.5% | 74.6 [74.3, 74.9], CV 0.3% |
| 64 | Standard | 13.5 [13.1, 14.3], CV 2.8% | 13.4 [13.3, 13.7], CV 0.8% | 68.0 [67.3, 69.8], CV 1.2% |
| 64 | mimalloc | 8.4 [8.2, 8.5], CV 1.6% | 8.7 [8.5, 9.0], CV 2.0% | 61.4 [61.0, 62.2], CV 0.6% |
| 64 | jemalloc | 19.7 [19.3, 20.4], CV 2.1% | 19.7 [19.4, 20.5], CV 1.6% | 74.9 [74.5, 75.9], CV 0.7% |
| 256 | Standard | 13.4 [13.1, 14.0], CV 2.1% | 13.3 [13.1, 13.4], CV 1.1% | 67.5 [66.9, 67.9], CV 0.5% |
| 256 | mimalloc | 9.0 [8.8, 9.3], CV 1.9% | 9.4 [9.1, 9.7], CV 2.5% | 62.8 [62.3, 64.6], CV 1.1% |
| 256 | jemalloc | 19.7 [19.2, 20.9], CV 3.2% | 19.6 [19.4, 19.8], CV 0.6% | 75.1 [74.7, 76.1], CV 0.6% |
| 4096 | Standard | 24.6 [24.3, 24.9], CV 0.8% | 24.6 [24.3, 24.9], CV 0.7% | 79.4 [78.8, 79.8], CV 0.4% |
| 4096 | mimalloc | 23.9 [23.6, 24.4], CV 1.2% | 23.9 [23.7, 24.1], CV 0.6% | 76.2 [75.5, 77.6], CV 0.8% |
| 4096 | jemalloc | 22.5 [22.3, 22.6], CV 0.4% | 22.6 [22.4, 22.9], CV 0.6% | 78.5 [78.1, 80.0], CV 0.8% |
| 65536 | Standard | 27.3 [27.1, 28.7], CV 1.8% | 27.1 [27.1, 27.2], CV 0.1% | 85.0 [81.6, 109.8], CV 10.3% |
| 65536 | mimalloc | 25.8 [25.4, 26.5], CV 1.4% | 25.5 [24.9, 27.7], CV 3.2% | 77.7 [77.2, 78.8], CV 0.8% |
| 65536 | jemalloc | 346.2 [275.2, 391.8], CV 10.7% | 360.0 [338.1, 367.1], CV 2.5% | 449.9 [431.0, 458.4], CV 1.6% |
<!-- /performance-table -->

### macOS

<!-- performance-table:latency-macos-arm64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 16.9 [15.7, 18.9], CV 6.8% | 17.3 [16.2, 20.0], CV 6.2% | 76.0 [64.5, 90.2], CV 10.9% |
| 16 | mimalloc | 5.5 [5.1, 5.8], CV 4.7% | 5.9 [5.4, 6.2], CV 5.1% | 61.5 [53.9, 67.1], CV 7.8% |
| 16 | jemalloc | 18.5 [15.9, 22.0], CV 11.2% | 19.1 [16.1, 27.6], CV 20.7% | 108.6 [102.8, 117.0], CV 4.5% |
| 64 | Standard | 21.0 [18.0, 24.7], CV 9.6% | 20.2 [18.7, 22.3], CV 5.9% | 78.6 [70.3, 87.3], CV 7.0% |
| 64 | mimalloc | 5.8 [5.3, 6.6], CV 8.0% | 6.3 [5.7, 7.4], CV 7.4% | 62.6 [54.0, 71.3], CV 8.7% |
| 64 | jemalloc | 18.1 [16.1, 22.5], CV 10.1% | 18.3 [16.2, 20.1], CV 8.2% | 130.6 [123.7, 142.2], CV 4.7% |
| 256 | Standard | 20.3 [16.9, 29.7], CV 17.7% | 20.0 [17.6, 23.6], CV 10.0% | 81.6 [72.8, 94.8], CV 8.8% |
| 256 | mimalloc | 7.2 [6.4, 9.0], CV 10.8% | 7.4 [6.8, 8.6], CV 8.7% | 65.3 [55.8, 82.6], CV 11.0% |
| 256 | jemalloc | 17.8 [15.3, 19.8], CV 6.7% | 18.1 [15.5, 21.8], CV 11.1% | 128.6 [120.7, 138.9], CV 4.6% |
| 4096 | Standard | 22.6 [19.8, 31.7], CV 15.0% | 23.5 [19.8, 28.0], CV 13.0% | 83.5 [73.0, 99.6], CV 10.2% |
| 4096 | mimalloc | 15.6 [14.4, 17.1], CV 5.5% | 16.2 [14.4, 20.2], CV 12.4% | 69.8 [60.9, 77.5], CV 7.4% |
| 4096 | jemalloc | 23.5 [20.8, 34.9], CV 18.7% | 23.8 [20.9, 27.6], CV 9.4% | 136.3 [126.9, 160.7], CV 7.1% |
| 65536 | Standard | 26.7 [22.4, 33.6], CV 12.5% | 26.7 [22.6, 33.6], CV 11.5% | 87.1 [73.9, 113.8], CV 14.7% |
| 65536 | mimalloc | 18.0 [16.7, 21.1], CV 7.5% | 17.9 [15.8, 20.2], CV 9.1% | 74.0 [64.9, 92.3], CV 13.4% |
| 65536 | jemalloc | 187.5 [171.0, 223.1], CV 8.6% | 191.0 [172.6, 209.1], CV 7.3% | 256.0 [232.4, 302.9], CV 7.3% |
<!-- /performance-table -->

Small differences may be measurement noise; counter cost varies by backend and size. Successful requests match in size and alignment.

## Cross-thread free

One thread allocates 64 B blocks; workers free them. Worker creation is outside timing; completion is inside.

<!-- performance-table:cross-thread -->
| Platform | Workers | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 46.7 [45.7, 48.8], CV 2.1% | 22.0 [21.7, 23.0], CV 1.8% | 66.9 [63.8, 69.5], CV 2.0% |
| Windows | 4 | 47.1 [45.1, 48.8], CV 2.3% | 23.2 [19.3, 25.3], CV 11.0% | 54.6 [52.7, 56.9], CV 2.7% |
| Windows | 8 | 53.8 [49.5, 79.9], CV 17.3% | 26.1 [24.1, 28.8], CV 5.9% | 58.4 [56.4, 59.8], CV 1.8% |
| Linux | 2 | 58.0 [35.3, 65.6], CV 14.3% | 24.3 [23.5, 25.1], CV 1.8% | 42.1 [41.6, 42.7], CV 0.9% |
| Linux | 4 | 55.6 [51.7, 62.8], CV 6.6% | 26.0 [24.6, 27.8], CV 4.0% | 43.2 [40.3, 45.4], CV 4.4% |
| Linux | 8 | 50.8 [29.0, 60.8], CV 19.4% | 26.1 [24.7, 27.8], CV 3.3% | 43.7 [42.5, 44.2], CV 1.5% |
| macOS | 2 | 4630.6 [73.4, 9600.3], CV 77.4% | 2927.9 [246.2, 15791.4], CV 165.3% | 4847.1 [206.9, 8348.4], CV 62.0% |
| macOS | 4 | 5016.6 [319.2, 13035.2], CV 98.5% | 9003.9 [514.2, 17816.6], CV 68.1% | 4460.5 [276.7, 14275.2], CV 119.5% |
| macOS | 8 | 5677.8 [193.5, 15319.7], CV 116.4% | 9728.0 [159.1, 23073.9], CV 100.0% | 7476.4 [156.5, 25598.6], CV 125.8% |
<!-- /performance-table -->

## Stack

Prepared Buffer, one mark/allocate/rewind cycle, alignment 16. Writes one byte; its work differs from Heap allocation.

<!-- performance-table:stack -->
| Platform | Bytes | ns/cycle |
| --- | --- | --- |
| Windows | 64 | 4.07 [4.07, 4.08], CV 0.1% |
| Windows | 4096 | 4.08 [4.07, 4.10], CV 0.2% |
| Linux | 64 | 5.16 [5.15, 5.17], CV 0.1% |
| Linux | 4096 | 5.16 [5.15, 5.23], CV 0.4% |
| macOS | 64 | 6.31 [5.92, 7.09], CV 5.7% |
| macOS | 4096 | 6.42 [5.82, 7.64], CV 8.8% |
<!-- /performance-table -->

CPU affinity is unpinned. These are complete-operation medians; individual-allocation p95/p99 are on the performance overview.

[Method](../benchmarking.md) · [Raw data](../results/current/README.md) · [Memory](memory.md)
