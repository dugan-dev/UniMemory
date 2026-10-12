# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-11 · [7e4b486](https://github.com/dugan-dev/UniMemory/actions/runs/38167630684) · [CSV](../results/current/native-applications/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators. The system allocator is glibc; threaded workloads use four workers. Trial counts and statistics are recorded above.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 2.888 [2.818, 3.120], CV 3.1% | 2.659 [2.618, 2.818], CV 2.5% | 2.723 [2.668, 2.918], CV 2.8% |
| espresso | 3.204 [3.168, 3.369], CV 2.1% | 2.864 [2.818, 2.968], CV 1.5% | 2.968 [2.868, 3.169], CV 3.4% |
| barnes | 1.642 [1.566, 1.717], CV 3.0% | 1.623 [1.567, 1.718], CV 2.7% | 1.621 [1.566, 1.717], CV 3.1% |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 5.841 [5.780, 6.032], CV 1.6% | 4.611 [4.528, 4.885], CV 2.3% | 4.615 [4.577, 4.831], CV 1.7% |
| cache-scratch | 0.466 [0.423, 0.474], CV 3.3% | 0.470 [0.466, 0.471], CV 0.4% | 0.471 [0.468, 0.473], CV 0.3% |
| cache-thrash | 0.505 [0.471, 0.524], CV 4.7% | 0.521 [0.516, 0.523], CV 0.4% | 0.516 [0.472, 0.522], CV 3.0% |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.003 [0.003, 0.004], CV 1.2% | 0.004 [0.004, 0.008], CV 35.1% | 0.007 [0.003, 0.008], CV 26.0% |
| malloc-large | 0.771 [0.766, 0.816], CV 2.1% | 0.626 [0.616, 0.666], CV 3.3% | 2.896 [2.871, 3.022], CV 1.7% |
| mstress | 0.258 [0.218, 0.270], CV 8.2% | 0.204 [0.166, 0.225], CV 11.6% | 0.378 [0.365, 0.417], CV 5.4% |
| mleak | 0.032 [0.032, 0.032], CV 0.2% | 0.065 [0.064, 0.067], CV 1.3% | 0.048 [0.032, 0.064], CV 35.4% |
| rptest | 16.035 [16.034, 16.035], CV 0.0% | 16.041 [16.038, 16.044], CV 0.0% | 16.040 [16.037, 16.044], CV 0.0% |
| glibc-simple | 2.027 [1.966, 2.167], CV 3.3% | 0.961 [0.915, 1.068], CV 4.6% | 1.276 [1.215, 1.366], CV 3.1% |
| glibc-thread | 2.026 [2.023, 2.029], CV 0.1% | 2.025 [2.022, 2.027], CV 0.1% | 2.024 [2.021, 2.027], CV 0.1% |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.27 [3.00, 3.34], CV 3.2% | 7.02 [6.95, 7.13], CV 0.9% | 10.71 [10.64, 10.77], CV 0.4% |
| espresso | 2.68 [2.63, 2.69], CV 1.0% | 12.57 [12.50, 12.62], CV 0.4% | 10.81 [10.73, 10.90], CV 0.5% |
| barnes | 60.69 [60.59, 60.83], CV 0.1% | 67.16 [67.09, 67.22], CV 0.1% | 74.63 [73.16, 75.31], CV 1.3% |
| larson | 46.43 [43.88, 49.07], CV 4.1% | 66.62 [64.75, 68.67], CV 1.6% | 70.53 [63.99, 74.20], CV 4.5% |
| larson-sized | 46.14 [40.81, 49.82], CV 6.0% | 66.76 [62.68, 68.80], CV 2.5% | 69.67 [64.67, 74.88], CV 5.1% |
| alloc-test | 14.83 [14.73, 14.94], CV 0.5% | 21.39 [21.30, 21.46], CV 0.3% | 27.36 [27.28, 27.43], CV 0.2% |
| cache-scratch | 3.95 [3.86, 4.04], CV 1.3% | 9.17 [9.12, 9.23], CV 0.5% | 10.60 [10.46, 10.74], CV 0.9% |
| cache-thrash | 3.91 [3.84, 4.05], CV 1.6% | 9.18 [9.12, 9.23], CV 0.5% | 10.59 [10.47, 10.65], CV 0.6% |
| xmalloc-test | 65.82 [61.22, 68.46], CV 3.4% | 66.77 [66.16, 72.17], CV 2.8% | 140.64 [120.74, 164.41], CV 10.1% |
| malloc-large-old | 3.91 [3.83, 3.95], CV 0.9% | 8.31 [8.29, 8.34], CV 0.2% | 10.63 [10.47, 10.66], CV 0.6% |
| malloc-large | 522.02 [521.93, 522.14], CV 0.0% | 625.17 [625.11, 625.19], CV 0.0% | 447.22 [446.59, 448.63], CV 0.2% |
| mstress | 79.39 [77.24, 82.62], CV 2.0% | 135.40 [114.38, 154.39], CV 8.1% | 71.68 [68.74, 74.90], CV 2.6% |
| mleak | 2.00 [1.91, 2.02], CV 1.7% | 6.45 [6.28, 6.53], CV 1.1% | 19.05 [18.56, 20.69], CV 4.5% |
| rptest | 18.09 [15.00, 23.21], CV 14.7% | 69.42 [65.07, 83.03], CV 7.9% | 90.11 [82.14, 100.98], CV 7.3% |
| glibc-simple | 1.98 [1.97, 2.04], CV 1.0% | 6.29 [6.28, 6.34], CV 0.4% | 14.59 [14.46, 14.66], CV 0.5% |
| glibc-thread | 2.58 [2.48, 2.66], CV 2.8% | 16.45 [16.39, 16.52], CV 0.2% | 26.35 [26.33, 26.38], CV 0.1% |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
