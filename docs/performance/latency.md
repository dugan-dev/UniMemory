# Allocation time

[Performance](../performance.md) · **English** · [简体中文](latency.zh-CN.md)

**Nanoseconds per complete operation; lower is faster.** Median of three fresh processes; Global, counters off, Release builds.

<!-- performance-source -->
2026-10-11 · [81cfacd](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240) · [CSV](../results/current/README.md)
<!-- /performance-source -->

## Object, container and block

### Windows

<!-- performance-table:workloads-windows-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 26.9 | 8.1 | 40.4 |
| make_shared, 64 B | 39.7 | 11.9 | 45.9 |
| vector, 32 uint64_t | 324.1 | 171.7 | 523.3 |
| PMR vector, 32 uint64_t | 333.0 | 183.1 | 528.1 |
| Zeroed Block, 4 KiB | 53.6 | 62.3 | 82.7 |
| Block resize, 4 to 8 KiB | 90.6 | 105.2 | 167.6 |
| Mixed lifetimes, 4-16 KiB | 94.9 | 29.4 | 117.4 |
| Cross-thread free, 64 B, 8 threads | 57.2 | 49.2 | 115.7 |
<!-- /performance-table -->

### Linux

<!-- performance-table:workloads-linux-x64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 13.0 | 7.0 | 13.6 |
| make_shared, 64 B | 15.1 | 10.9 | 21.2 |
| vector, 32 uint64_t | 117.0 | 77.9 | 148.3 |
| PMR vector, 32 uint64_t | 128.9 | 87.2 | 161.1 |
| Zeroed Block, 4 KiB | 103.0 | 89.3 | 89.4 |
| Block resize, 4 to 8 KiB | 135.3 | 102.7 | 125.7 |
| Mixed lifetimes, 4-16 KiB | 143.6 | 25.8 | 82.7 |
| Cross-thread free, 64 B, 8 threads | 129.9 | 34.8 | 73.3 |
<!-- /performance-table -->

### macOS

<!-- performance-table:workloads-macos-arm64 -->
| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 18.5 | 4.8 | 16.8 |
| make_shared, 64 B | 19.0 | 10.5 | 19.2 |
| vector, 32 uint64_t | 137.5 | 76.7 | 167.0 |
| PMR vector, 32 uint64_t | 144.5 | 106.5 | 172.4 |
| Zeroed Block, 4 KiB | 73.4 | 65.9 | 76.8 |
| Block resize, 4 to 8 KiB | 137.8 | 115.2 | 137.4 |
| Mixed lifetimes, 4-16 KiB | 32.5 | 22.8 | 28.2 |
| Cross-thread free, 64 B, 8 threads | 27.7 | 29.6 | 80.7 |
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
| 16 | Standard | 28.7 | 29.3 | 47.2 |
| 16 | mimalloc | 7.3 | 7.2 | 30.5 |
| 16 | jemalloc | 38.3 | 39.7 | 43.8 |
| 64 | Standard | 28.8 | 29.6 | 47.5 |
| 64 | mimalloc | 6.7 | 7.6 | 30.8 |
| 64 | jemalloc | 37.0 | 38.1 | 44.4 |
| 256 | Standard | 31.0 | 29.4 | 49.2 |
| 256 | mimalloc | 7.8 | 8.5 | 31.8 |
| 256 | jemalloc | 38.9 | 38.2 | 41.8 |
| 4096 | Standard | 30.7 | 29.5 | 50.5 |
| 4096 | mimalloc | 28.7 | 29.4 | 50.9 |
| 4096 | jemalloc | 41.4 | 43.3 | 46.7 |
| 65536 | Standard | 85.4 | 89.0 | 126.7 |
| 65536 | mimalloc | 31.6 | 32.2 | 53.2 |
| 65536 | jemalloc | 325.8 | 332.1 | 333.4 |
<!-- /performance-table -->

### Linux

<!-- performance-table:latency-linux-x64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 16.9 | 17.7 | 18.5 |
| 16 | mimalloc | 8.6 | 9.2 | 9.8 |
| 16 | jemalloc | 19.1 | 19.7 | 21.8 |
| 64 | Standard | 17.0 | 17.7 | 18.4 |
| 64 | mimalloc | 9.0 | 11.9 | 9.9 |
| 64 | jemalloc | 19.9 | 19.7 | 22.4 |
| 256 | Standard | 16.9 | 17.8 | 18.4 |
| 256 | mimalloc | 9.5 | 13.3 | 9.4 |
| 256 | jemalloc | 18.9 | 19.5 | 22.0 |
| 4096 | Standard | 29.9 | 30.3 | 31.2 |
| 4096 | mimalloc | 23.9 | 24.3 | 18.7 |
| 4096 | jemalloc | 22.1 | 22.7 | 24.9 |
| 65536 | Standard | 32.4 | 33.2 | 34.0 |
| 65536 | mimalloc | 25.2 | 25.8 | 19.3 |
| 65536 | jemalloc | 378.6 | 390.7 | 365.4 |
<!-- /performance-table -->

### macOS

<!-- performance-table:latency-macos-arm64 -->
| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 16.6 | 17.0 | 18.9 |
| 16 | mimalloc | 5.8 | 6.2 | 6.8 |
| 16 | jemalloc | 20.9 | 17.6 | 18.9 |
| 64 | Standard | 18.3 | 20.0 | 21.4 |
| 64 | mimalloc | 5.9 | 6.1 | 7.6 |
| 64 | jemalloc | 18.4 | 25.0 | 21.7 |
| 256 | Standard | 18.8 | 19.1 | 20.8 |
| 256 | mimalloc | 7.0 | 7.5 | 7.5 |
| 256 | jemalloc | 19.6 | 17.4 | 18.6 |
| 4096 | Standard | 22.5 | 22.8 | 25.0 |
| 4096 | mimalloc | 16.5 | 18.4 | 16.9 |
| 4096 | jemalloc | 23.1 | 24.2 | 24.6 |
| 65536 | Standard | 24.6 | 25.0 | 26.9 |
| 65536 | mimalloc | 17.2 | 17.9 | 18.0 |
| 65536 | jemalloc | 282.2 | 291.3 | 242.8 |
<!-- /performance-table -->

Small differences may be measurement noise; counter cost varies by backend and size. Successful requests match in size and alignment.

## Cross-thread free

One thread allocates 64 B blocks; workers free them. Worker creation is outside timing; completion is inside.

<!-- performance-table:cross-thread -->
| Platform | Workers | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 66.1 | 29.7 | 93.8 |
| Windows | 4 | 55.6 | 36.0 | 102.5 |
| Windows | 8 | 57.2 | 49.2 | 115.7 |
| Linux | 2 | 112.5 | 24.2 | 48.2 |
| Linux | 4 | 122.3 | 26.5 | 59.9 |
| Linux | 8 | 129.9 | 34.8 | 73.3 |
| macOS | 2 | 23.7 | 26.2 | 43.1 |
| macOS | 4 | 24.8 | 25.0 | 55.6 |
| macOS | 8 | 27.7 | 29.6 | 80.7 |
<!-- /performance-table -->

## Stack

Prepared Buffer, one mark/allocate/rewind cycle, alignment 16. Writes one byte; its work differs from Heap allocation.

<!-- performance-table:stack -->
| Platform | Bytes | ns/cycle |
| --- | --- | --- |
| Windows | 64 | 2.17 |
| Windows | 4096 | 2.17 |
| Linux | 64 | 4.67 |
| Linux | 4096 | 4.66 |
| macOS | 64 | 3.86 |
| macOS | 4096 | 3.76 |
<!-- /performance-table -->

CPU affinity is unpinned. These are complete-operation medians; individual-allocation p95/p99 are on the performance overview.

[Method](../benchmarking.md) · [Raw data](../results/current/README.md) · [Memory](memory.md)
