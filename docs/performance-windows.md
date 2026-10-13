# Windows performance validation

Windows x64 · MSVC 19.44 · C++20 · Release · /O2 /Ob3 · statistics off · 2026-10-13. Recorded profile: Release IPO=ON, diagnostic target IPO=OFF. Ten independent-process trials per scenario, alternating Native/API order; 125 ms calibration target. All samples are retained.

This measurement uses directly constructed Global contexts, with construction outside timing. Before-migration reference: `69651a1885c3417fa2f0751f04d3e6ecdea6cf25`. Both builds use the same timing kernels, SDKs and compiler settings; this is local evidence, not remote CI validation.

Median durations in ns; lower is better. Raw pairs use 16-byte alignment and include deallocation; other rows cover complete operations. Vector includes reserve(256), 256 uint64_t push_back calls and destruction.

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

## Compared with before migration

| Backend | Faster / total scenarios | Equal-weight geometric mean speedup |
| --- | ---: | ---: |
| standard | 8 / 14 | 2.1% |
| mimalloc | 13 / 14 | 6.6% |
| jemalloc | 7 / 14 | -2.9% |

Actual median durations improve in 28 of 42 scenarios; small differences may be measurement noise.

## Remaining cases below the 90% native threshold

The gate checks both paired medians and ratios of arithmetic mean durations; 39/42 pass. All failing results are retained and the threshold is unchanged.

| Backend | Scenario | Paired median | Mean-duration ratio |
| --- | --- | ---: | ---: |
| standard | Vector, 256 64-bit integers | 81.18% | 79.94% |
| mimalloc | Allocate + free 16 B | 88.78% | 89.56% |
| mimalloc | OwnedBlock, 64 B | 88.23% | 88.55% |

## Correctness

Local migration validation: 5,337 source CTest cases and 54 installed-consumer CTest cases passed.

[Table data and configuration records](results/windows-local-comparison.json). The README chart retains previously published CI measurements and was not regenerated.
