# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-11 · [e758dc2](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387) · [CSV](../results/current/native-applications/README.md)
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators × 3 processes: 144 successful runs. Each row is the median of three processes; the system allocator is glibc and threaded workloads use four workers.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.672 | 3.323 | 3.422 |
| espresso | 4.073 | 3.622 | 3.674 |
| barnes | 2.420 | 2.318 | 2.370 |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 4.224 | 3.277 | 3.226 |
| cache-scratch | 0.566 | 0.618 | 0.566 |
| cache-thrash | 0.568 | 0.616 | 0.566 |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 | 0.004 | 0.008 |
| malloc-large | 1.218 | 1.118 | 3.424 |
| mstress | 0.214 | 0.216 | 0.416 |
| mleak | 0.032 | 0.064 | 0.064 |
| rptest | 16.046 | 16.045 | 16.045 |
| glibc-simple | 2.218 | 1.617 | 2.019 |
| glibc-thread | 2.023 | 2.022 | 2.023 |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.32 | 7.06 | 10.71 |
| espresso | 2.69 | 12.56 | 10.80 |
| barnes | 60.67 | 67.23 | 73.30 |
| larson | 41.73 | 62.73 | 69.45 |
| larson-sized | 39.25 | 62.72 | 71.75 |
| alloc-test | 14.82 | 21.33 | 29.21 |
| cache-scratch | 3.94 | 9.12 | 10.63 |
| cache-thrash | 3.87 | 9.23 | 10.63 |
| xmalloc-test | 58.69 | 66.17 | 120.62 |
| malloc-large-old | 3.90 | 8.30 | 10.62 |
| malloc-large | 522.01 | 625.16 | 448.61 |
| mstress | 68.27 | 126.40 | 67.15 |
| mleak | 1.91 | 6.21 | 18.52 |
| rptest | 13.08 | 28.98 | 39.34 |
| glibc-simple | 1.85 | 6.27 | 14.64 |
| glibc-thread | 2.35 | 16.39 | 26.38 |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All 144 raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
