# Allocation time

[Performance](../performance.md) · **English** · [简体中文](latency.zh-CN.md)

**Nanoseconds per complete operation; lower is faster.** Median of three process trials; Global allocation, counters off.

## 1 · Object / Container / Block

### Windows / MSVC

| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 46.4 | 12.2 | 36.0 |
| make_shared, 64 B | 63.1 | 23.7 | 56.5 |
| vector, 32 integers | 590.3 | 237.4 | 488.5 |
| PMR vector, 32 integers | 580.5 | 253.6 | 494.7 |
| Zeroed Block, 4 KiB | 81.3 | 52.1 | 70.7 |
| Block resize, 4 to 8 KiB | 168.5 | 139.8 | 149.4 |
| Mixed lifetimes, 4-16 KiB | 152.0 | 44.9 | 138.5 |
| Cross-thread free, 8 threads | 105.4 | 59.8 | 166.6 |

### Linux / GCC / WSL

| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 18.8 | 10.6 | 20.0 |
| make_shared, 64 B | 20.8 | 13.3 | 27.3 |
| vector, 32 integers | 153.2 | 111.5 | 190.4 |
| PMR vector, 32 integers | 171.8 | 131.6 | 207.0 |
| Zeroed Block, 4 KiB | 72.0 | 59.9 | 65.1 |
| Block resize, 4 to 8 KiB | 119.1 | 94.6 | 111.4 |
| Mixed lifetimes, 4-16 KiB | 134.6 | 35.0 | 93.1 |
| Cross-thread free, 8 threads | 139.3 | 53.5 | 151.4 |

Object includes construction/destruction; vector includes growth/destruction; resize includes allocation, growth and release. Compare within a row.

![Backend workload comparison](../images/workload-comparison.png)

Standard = 1 within each workload; shorter is faster.

## 2 · Native / API / Basic

One allocation/free pair, alignment 16. Native calls the backend directly; API uses UniMemory with counters off; Basic enables counters.

### Windows

| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 64 | Standard | 42.3 | 44.4 | 72.3 |
| 64 | mimalloc | 7.1 | 11.2 | 33.5 |
| 64 | jemalloc | 36.2 | 38.5 | 65.6 |
| 4096 | Standard | 44.7 | 45.4 | 70.8 |
| 4096 | mimalloc | 28.2 | 32.5 | 54.3 |
| 4096 | jemalloc | 42.6 | 43.7 | 71.5 |

### Linux / WSL

| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 64 | Standard | 15.9 | 21.0 | 44.1 |
| 64 | mimalloc | 9.0 | 12.2 | 34.3 |
| 64 | jemalloc | 18.1 | 24.1 | 44.1 |
| 4096 | Standard | 28.1 | 34.5 | 54.6 |
| 4096 | mimalloc | 29.9 | 31.6 | 58.2 |
| 4096 | jemalloc | 21.0 | 29.5 | 47.9 |

Disabling counters avoids counting overhead; the unified API still adds call overhead. Successful requests match in size/alignment; failure handling differs. See the [measurement method](../benchmarking.md).

## 3 · Cross-thread free

One thread allocates; workers free. Worker creation is outside timing; completion is inside.

| Platform | Workers | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 118.7 | 53.8 | 81.5 |
| Windows | 4 | 101.2 | 49.5 | 100.1 |
| Windows | 8 | 105.4 | 59.8 | 166.6 |
| Linux | 2 | 125.9 | 48.4 | 54.4 |
| Linux | 4 | 124.8 | 42.5 | 76.3 |
| Linux | 8 | 139.3 | 53.5 | 151.4 |

## 4 · Stack

Prepared Buffer, one mark/allocate/rewind cycle. Writes one byte; this is not an equivalent Heap-allocation workload.

| Platform | Bytes | ns/cycle |
| --- | --- | --- |
| Windows | 64 | 3.49 |
| Windows | 4096 | 3.58 |
| Linux | 64 | 4.13 |
| Linux | 4096 | 4.15 |

CPU affinity is unpinned; small differences may be noise. Raw latency files retain within-process min/median/max and all three trials; these are not p95/p99 results.

[Method](../benchmarking.md) · [Raw data](../results/0.0.1/README.md) · [Memory](memory.md)
