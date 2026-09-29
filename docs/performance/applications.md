# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

Median of three processes per program. The system allocator here is glibc.

## 1 · Elapsed time

**Seconds, lower is better.**

| Program | glibc | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| cfrac | 4.81 | 4.55 | 4.55 |
| espresso | 6.01 | 5.58 | 5.45 |
| barnes | 2.48 | 2.51 | 2.50 |
| malloc-large | 3.43 | 2.82 | 15.28 |
| mstress | 0.32 | 0.27 | 0.53 |
| cache-thrash | 0.42 | 0.43 | 0.38 |

## 2 · Peak memory

**Peak RSS, MiB, lower is better.**

| Program | glibc | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| cfrac | 2.81 | 6.05 | 4.69 |
| espresso | 1.88 | 12.04 | 4.69 |
| barnes | 56.25 | 64.95 | 58.12 |
| malloc-large | 521.64 | 595.15 | 428.38 |
| mstress | 74.57 | 137.14 | 48.46 |
| cache-thrash | 2.81 | 8.02 | 3.75 |

## 3 · Interpretation

For malloc-large, mimalloc has lower elapsed time while jemalloc has lower peak memory. **Faster need not mean less memory.**

The collection includes numerical, logic, large-block, cross-thread and cache workloads. All **144 runs** across 16 programs remain available; six are shown here. Fixed-duration Larson programs are not ranked by elapsed time.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time's OS accounting, with platform limits; it differs from the resident-page snapshots in the memory report.

[Raw data](../results/0.0.1/README.md) · [Reproduce](../benchmarking.md)
