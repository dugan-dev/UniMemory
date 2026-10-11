# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-11 · [81cfacd](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240) · [CSV](../results/current/native-applications/README.md)
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators × 3 processes: 144 successful runs. Each row is the median of three processes; the system allocator is glibc and threaded workloads use four workers.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.672 | 3.322 | 3.422 |
| espresso | 4.071 | 3.622 | 3.673 |
| barnes | 2.319 | 2.320 | 2.318 |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 4.224 | 3.170 | 3.222 |
| cache-scratch | 0.568 | 0.616 | 0.565 |
| cache-thrash | 0.565 | 0.619 | 0.566 |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 | 0.004 | 0.008 |
| malloc-large | 1.168 | 1.067 | 3.323 |
| mstress | 0.216 | 0.166 | 0.365 |
| mleak | 0.032 | 0.064 | 0.064 |
| rptest | 16.042 | 16.043 | 16.043 |
| glibc-simple | 2.219 | 1.667 | 2.018 |
| glibc-thread | 2.022 | 2.023 | 2.020 |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.29 | 7.07 | 10.73 |
| espresso | 2.63 | 12.62 | 10.80 |
| barnes | 60.70 | 67.15 | 75.14 |
| larson | 41.92 | 64.85 | 70.51 |
| larson-sized | 39.83 | 64.77 | 72.31 |
| alloc-test | 14.77 | 21.42 | 30.90 |
| cache-scratch | 3.89 | 9.23 | 10.64 |
| cache-thrash | 3.88 | 9.16 | 10.64 |
| xmalloc-test | 55.04 | 66.17 | 130.69 |
| malloc-large-old | 3.90 | 8.30 | 10.52 |
| malloc-large | 521.96 | 625.15 | 446.62 |
| mstress | 67.80 | 156.39 | 67.08 |
| mleak | 1.91 | 6.27 | 26.54 |
| rptest | 13.53 | 29.11 | 40.07 |
| glibc-simple | 1.85 | 6.28 | 14.66 |
| glibc-thread | 2.35 | 16.45 | 26.34 |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All 144 raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
