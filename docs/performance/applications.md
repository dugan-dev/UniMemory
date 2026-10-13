# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-13 · [c917e81](https://github.com/dugan-dev/UniMemory/actions/runs/38415666741) · [CSV](../results/current/native-applications/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators. The system allocator is glibc; threaded workloads use four workers. Trial counts and statistics are recorded above.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 4.610 [4.574, 4.674], CV 0.7% | 4.189 [4.173, 4.274], CV 0.8% | 4.334 [4.274, 4.375], CV 0.7% |
| espresso | 5.002 [4.975, 5.075], CV 0.7% | 4.441 [4.424, 4.476], CV 0.5% | 4.564 [4.524, 4.577], CV 0.5% |
| barnes | 3.266 [3.221, 3.322], CV 1.1% | 3.283 [3.223, 3.325], CV 1.0% | 3.277 [3.221, 3.323], CV 1.1% |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 7.854 [7.786, 7.940], CV 0.6% | 6.182 [6.134, 6.290], CV 0.8% | 6.242 [6.184, 6.442], CV 1.4% |
| cache-scratch | 0.670 [0.667, 0.674], CV 0.4% | 0.690 [0.668, 0.721], CV 3.8% | 0.670 [0.668, 0.673], CV 0.3% |
| cache-thrash | 0.670 [0.668, 0.673], CV 0.3% | 0.715 [0.674, 0.725], CV 2.1% | 0.670 [0.668, 0.675], CV 0.4% |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 [0.004, 0.004], CV 1.3% | 0.007 [0.004, 0.008], CV 22.8% | 0.008 [0.008, 0.008], CV 0.8% |
| malloc-large | 1.804 [1.719, 1.920], CV 3.2% | 1.650 [1.571, 1.772], CV 3.6% | 3.976 [3.925, 4.076], CV 1.6% |
| mstress | 0.324 [0.317, 0.368], CV 4.8% | 0.271 [0.268, 0.273], CV 0.6% | 0.495 [0.466, 0.520], CV 5.0% |
| mleak | 0.064 [0.064, 0.064], CV 0.3% | 0.065 [0.064, 0.067], CV 1.2% | 0.064 [0.064, 0.065], CV 0.4% |
| rptest | 16.044 [16.041, 16.050], CV 0.0% | 16.049 [16.045, 16.055], CV 0.0% | 16.049 [16.045, 16.055], CV 0.0% |
| glibc-simple | 2.955 [2.919, 3.020], CV 1.1% | 2.069 [2.067, 2.070], CV 0.1% | 2.534 [2.518, 2.570], CV 1.0% |
| glibc-thread | 2.025 [2.021, 2.027], CV 0.1% | 2.025 [2.022, 2.029], CV 0.1% | 2.025 [2.023, 2.028], CV 0.1% |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.10 [2.93, 3.20], CV 2.6% | 7.08 [7.02, 7.14], CV 0.8% | 10.61 [10.50, 10.68], CV 0.6% |
| espresso | 2.57 [2.56, 2.57], CV 0.1% | 12.60 [12.50, 12.63], CV 0.3% | 10.66 [10.49, 10.75], CV 0.9% |
| barnes | 60.79 [60.46, 62.48], CV 1.0% | 67.14 [67.07, 67.18], CV 0.1% | 74.25 [72.92, 75.20], CV 1.4% |
| larson | 40.03 [37.66, 43.75], CV 4.8% | 62.24 [60.25, 64.23], CV 1.5% | 64.33 [59.83, 67.12], CV 4.2% |
| larson-sized | 39.13 [37.61, 42.34], CV 5.0% | 62.61 [60.11, 66.23], CV 3.3% | 65.02 [62.44, 67.40], CV 2.7% |
| alloc-test | 14.76 [14.72, 14.85], CV 0.4% | 21.34 [21.28, 21.39], CV 0.2% | 28.09 [27.24, 31.24], CV 5.7% |
| cache-scratch | 3.81 [3.71, 3.91], CV 1.8% | 9.16 [9.11, 9.25], CV 0.6% | 10.57 [10.55, 10.59], CV 0.2% |
| cache-thrash | 3.84 [3.77, 3.91], CV 1.6% | 9.21 [9.12, 9.25], CV 0.5% | 10.54 [10.40, 10.60], CV 0.6% |
| xmalloc-test | 63.67 [59.90, 67.49], CV 3.5% | 65.97 [65.92, 66.04], CV 0.1% | 125.25 [113.81, 136.96], CV 5.7% |
| malloc-large-old | 3.82 [3.70, 3.87], CV 1.6% | 8.16 [8.06, 8.26], CV 1.1% | 10.47 [10.39, 10.55], CV 0.5% |
| malloc-large | 521.91 [521.87, 521.98], CV 0.0% | 621.93 [617.24, 623.18], CV 0.3% | 448.04 [446.42, 450.45], CV 0.4% |
| mstress | 73.91 [68.12, 79.07], CV 5.6% | 139.96 [114.43, 166.33], CV 13.1% | 68.93 [66.53, 72.98], CV 3.5% |
| mleak | 1.96 [1.95, 2.07], CV 1.9% | 6.29 [6.03, 6.53], CV 2.9% | 19.96 [18.45, 22.60], CV 9.6% |
| rptest | 21.54 [15.87, 30.63], CV 24.1% | 65.62 [58.95, 69.07], CV 5.4% | 83.34 [75.64, 99.12], CV 8.4% |
| glibc-simple | 1.85 [1.85, 1.85], CV 0.0% | 6.39 [6.27, 6.41], CV 0.6% | 14.44 [14.27, 14.56], CV 0.5% |
| glibc-thread | 2.41 [2.41, 2.41], CV 0.1% | 16.45 [16.39, 16.51], CV 0.2% | 26.31 [26.28, 26.32], CV 0.1% |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
