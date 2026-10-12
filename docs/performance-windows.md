# Windows performance validation

Windows x64 · MSVC 19.44 · C++20 · Release · IPO · /MD · statistics off · 2026-10-12。Ten independent-process trials per scenario, alternating Native/API order; 250 ms calibration target. All samples are retained.

main 对照：`86ccb37c9ac25b5288e4bf8a73bd85a1d04e1c5d`。Both builds use identical timing kernels, fastest valid native APIs, SDKs and compiler settings.

Median durations in ns; lower is better. Raw pairs use 16-byte alignment and include deallocation; other rows cover complete operations.

| Scenario | std | std-unim | mi | mi-unim |
| --- | ---: | ---: | ---: | ---: |
| Allocate + free 16 B | 28.45 | 28.56 | 5.37 | 6.28 |
| Allocate + free 64 B | 28.38 | 28.47 | 5.74 | 6.49 |
| Allocate + free 256 B | 28.94 | 29.36 | 6.87 | 7.40 |
| Allocate + free 4 KiB | 28.58 | 29.48 | 23.83 | 24.85 |
| Allocate + free 64 KiB | 95.39 | 94.98 | 25.14 | 25.70 |
| Zeroed allocation | 29.73 | 31.15 | 8.57 | 9.41 |
| Reallocate | 59.49 | 58.34 | 15.23 | 16.11 |
| Reallocate with zeroed growth | 58.90 | 59.61 | 16.00 | 16.60 |
| Unique object | 30.63 | 30.77 | 5.83 | 6.13 |
| Object array | 43.54 | 42.88 | 20.33 | 20.13 |
| Shared object | 55.07 | 55.89 | 29.45 | 30.99 |
| Vector | 192.39 | 233.17 | 228.04 | 242.75 |
| PMR Vector | 612.96 | 501.56 | 517.86 | 626.49 |
| OwnedBlock | 30.10 | 29.89 | 4.82 | 5.66 |

## Compared with main

| Backend | Faster / total scenarios | Equal-weight geometric mean speedup |
| --- | ---: | ---: |
| standard | 11 / 14 | 9.0% |
| mimalloc | 6 / 14 | 3.4% |
| jemalloc | 10 / 14 | 8.6% |

Actual median durations improve in 27 of 42 scenarios; small differences may be noise rather than significant gains.

## Remaining cases below the 90% native threshold

The gate checks both paired medians and ratios of arithmetic mean durations; 38/42 pass. All failing results are retained and the threshold is unchanged.

| Backend | Scenario | Paired median | Mean-duration ratio |
| --- | --- | ---: | ---: |
| standard | Vector | 83.14% | 82.78% |
| mimalloc | Allocate + free 16 B | 86.02% | 85.94% |
| mimalloc | Allocate + free 64 B | 88.05% | 88.84% |
| mimalloc | OwnedBlock | 85.92% | 85.86% |

## Correctness

Windows 三Backend × Debug/Release × 统计开关：9,108 项源码 CTest 与 108 项安装消费者 CTest 全部通过。另验证了 Heap/Stack 使用与跨 DLL Global/OOM 行为。

Raw measurements, calibration, compiler settings and SDK/executable hashes are retained; GitHub Actions uploads complete performance artifacts.
