# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-11 · [8365472](https://github.com/dugan-dev/UniMemory/actions/runs/38167739252) · [CSV](../results/current/native-applications/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators. The system allocator is glibc; threaded workloads use four workers. Trial counts and statistics are recorded above.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.679 [3.672, 3.725], CV 0.4% | 3.339 [3.323, 3.477], CV 1.4% | 3.433 [3.422, 3.522], CV 0.9% |
| espresso | 4.075 [4.073, 4.076], CV 0.0% | 3.624 [3.622, 3.625], CV 0.0% | 3.678 [3.672, 3.724], CV 0.4% |
| barnes | 2.405 [2.369, 2.471], CV 1.4% | 2.377 [2.321, 2.423], CV 1.6% | 2.370 [2.319, 2.421], CV 1.4% |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 6.868 [6.589, 7.145], CV 2.7% | 5.152 [5.035, 5.388], CV 2.5% | 5.218 [5.083, 5.540], CV 3.2% |
| cache-scratch | 0.571 [0.568, 0.573], CV 0.3% | 0.622 [0.619, 0.624], CV 0.3% | 0.572 [0.569, 0.574], CV 0.2% |
| cache-thrash | 0.572 [0.570, 0.574], CV 0.3% | 0.621 [0.619, 0.624], CV 0.3% | 0.571 [0.567, 0.573], CV 0.4% |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 [0.004, 0.004], CV 0.6% | 0.004 [0.004, 0.008], CV 29.9% | 0.007 [0.003, 0.008], CV 18.5% |
| malloc-large | 1.268 [1.218, 1.369], CV 3.7% | 1.175 [1.118, 1.268], CV 4.2% | 3.474 [3.424, 3.625], CV 2.0% |
| mstress | 0.294 [0.267, 0.322], CV 8.7% | 0.262 [0.219, 0.270], CV 5.9% | 0.455 [0.418, 0.474], CV 5.3% |
| mleak | 0.032 [0.032, 0.032], CV 0.2% | 0.065 [0.064, 0.066], CV 1.2% | 0.064 [0.064, 0.064], CV 0.2% |
| rptest | 16.044 [16.042, 16.046], CV 0.0% | 16.048 [16.044, 16.052], CV 0.0% | 16.049 [16.045, 16.053], CV 0.0% |
| glibc-simple | 2.219 [2.219, 2.220], CV 0.0% | 1.619 [1.617, 1.620], CV 0.1% | 2.024 [2.018, 2.069], CV 0.8% |
| glibc-thread | 2.027 [2.024, 2.030], CV 0.1% | 2.026 [2.023, 2.031], CV 0.1% | 2.027 [2.025, 2.028], CV 0.1% |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.26 [3.12, 3.36], CV 2.5% | 7.01 [6.96, 7.13], CV 0.9% | 10.72 [10.63, 10.82], CV 0.5% |
| espresso | 2.69 [2.69, 2.69], CV 0.0% | 12.57 [12.50, 12.63], CV 0.3% | 10.77 [10.66, 10.88], CV 0.6% |
| barnes | 60.70 [60.57, 60.83], CV 0.1% | 67.16 [67.07, 67.22], CV 0.1% | 74.43 [73.16, 75.30], CV 1.4% |
| larson | 42.60 [39.23, 43.85], CV 3.3% | 63.98 [62.19, 66.47], CV 2.9% | 67.29 [60.50, 71.64], CV 4.3% |
| larson-sized | 43.83 [42.39, 46.91], CV 3.1% | 65.44 [62.47, 68.43], CV 2.6% | 67.74 [60.50, 71.51], CV 5.1% |
| alloc-test | 14.85 [14.73, 14.94], CV 0.5% | 21.36 [21.31, 21.45], CV 0.2% | 27.40 [27.32, 27.43], CV 0.1% |
| cache-scratch | 3.95 [3.84, 4.03], CV 1.7% | 9.18 [9.11, 9.25], CV 0.5% | 10.62 [10.46, 10.73], CV 0.6% |
| cache-thrash | 3.94 [3.84, 4.05], CV 1.8% | 9.16 [8.99, 9.24], CV 0.9% | 10.59 [10.46, 10.66], CV 0.8% |
| xmalloc-test | 65.57 [61.77, 68.88], CV 3.5% | 72.56 [66.11, 98.17], CV 18.6% | 131.87 [117.22, 156.70], CV 8.0% |
| malloc-large-old | 3.89 [3.79, 3.96], CV 1.6% | 8.27 [8.18, 8.33], CV 0.6% | 10.62 [10.47, 10.66], CV 0.5% |
| malloc-large | 522.03 [521.93, 522.18], CV 0.0% | 625.18 [625.16, 625.24], CV 0.0% | 448.43 [446.62, 450.64], CV 0.3% |
| mstress | 74.23 [68.11, 80.12], CV 5.7% | 129.03 [114.43, 158.45], CV 10.4% | 68.54 [64.05, 72.21], CV 3.1% |
| mleak | 2.03 [2.01, 2.14], CV 1.9% | 6.45 [6.27, 6.52], CV 1.6% | 19.87 [18.59, 24.64], CV 10.7% |
| rptest | 17.97 [16.11, 20.55], CV 7.6% | 65.40 [61.11, 68.95], CV 4.0% | 79.90 [70.21, 87.61], CV 6.7% |
| glibc-simple | 1.97 [1.97, 1.97], CV 0.0% | 6.30 [6.27, 6.34], CV 0.5% | 14.78 [14.38, 16.64], CV 4.5% |
| glibc-thread | 2.53 [2.47, 2.60], CV 1.4% | 16.43 [16.38, 16.45], CV 0.2% | 26.56 [26.33, 28.38], CV 2.4% |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
