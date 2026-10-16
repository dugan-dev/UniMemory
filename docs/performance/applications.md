# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-13 · [c917e81](https://github.com/dugan-dev/UniMemory/actions/runs/38415569101) · [CSV](../results/current/native-applications/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators. The system allocator is glibc; threaded workloads use four workers. Trial counts and statistics are recorded above.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 4.610 [4.574, 4.675], CV 0.7% | 4.185 [4.126, 4.225], CV 0.7% | 4.324 [4.274, 4.375], CV 0.5% |
| espresso | 5.031 [5.025, 5.075], CV 0.3% | 4.461 [4.424, 4.527], CV 0.8% | 4.574 [4.574, 4.575], CV 0.0% |
| barnes | 3.292 [3.271, 3.322], CV 0.8% | 3.308 [3.272, 3.373], CV 1.0% | 3.302 [3.271, 3.322], CV 0.8% |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 7.886 [7.839, 7.893], CV 0.2% | 6.218 [6.184, 6.241], CV 0.4% | 6.247 [6.235, 6.288], CV 0.3% |
| cache-scratch | 0.670 [0.669, 0.672], CV 0.2% | 0.686 [0.670, 0.720], CV 3.3% | 0.670 [0.670, 0.672], CV 0.1% |
| cache-thrash | 0.670 [0.667, 0.671], CV 0.2% | 0.715 [0.673, 0.728], CV 2.1% | 0.670 [0.668, 0.672], CV 0.2% |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 [0.004, 0.004], CV 0.7% | 0.007 [0.004, 0.008], CV 16.8% | 0.008 [0.006, 0.008], CV 8.4% |
| malloc-large | 1.894 [1.819, 1.970], CV 2.6% | 1.776 [1.719, 1.871], CV 2.8% | 4.196 [4.125, 4.326], CV 1.5% |
| mstress | 0.323 [0.316, 0.366], CV 4.8% | 0.280 [0.266, 0.324], CV 8.0% | 0.510 [0.469, 0.523], CV 4.3% |
| mleak | 0.064 [0.064, 0.065], CV 0.3% | 0.065 [0.064, 0.066], CV 1.4% | 0.069 [0.064, 0.115], CV 23.2% |
| rptest | 16.045 [16.043, 16.048], CV 0.0% | 16.050 [16.048, 16.053], CV 0.0% | 16.049 [16.047, 16.052], CV 0.0% |
| glibc-simple | 2.940 [2.919, 3.020], CV 1.2% | 2.080 [2.068, 2.120], CV 1.0% | 2.539 [2.519, 2.619], CV 1.4% |
| glibc-thread | 2.025 [2.022, 2.027], CV 0.1% | 2.025 [2.022, 2.029], CV 0.1% | 2.025 [2.023, 2.032], CV 0.1% |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.12 [2.95, 3.26], CV 3.0% | 7.05 [7.02, 7.13], CV 0.6% | 10.59 [10.52, 10.70], CV 0.7% |
| espresso | 2.55 [2.44, 2.57], CV 1.5% | 12.58 [12.50, 12.63], CV 0.3% | 10.69 [10.59, 10.75], CV 0.6% |
| barnes | 60.96 [60.46, 62.51], CV 1.3% | 67.13 [67.06, 67.27], CV 0.1% | 74.67 [73.09, 75.11], CV 1.1% |
| larson | 40.18 [36.89, 42.23], CV 4.3% | 62.51 [60.12, 64.22], CV 1.9% | 65.36 [59.62, 68.34], CV 3.7% |
| larson-sized | 39.05 [37.54, 42.34], CV 4.1% | 63.29 [60.22, 68.13], CV 3.6% | 65.55 [58.96, 68.63], CV 4.4% |
| alloc-test | 14.76 [14.69, 14.85], CV 0.5% | 21.34 [21.22, 21.39], CV 0.2% | 27.72 [27.30, 29.36], CV 2.9% |
| cache-scratch | 3.82 [3.71, 3.92], CV 1.7% | 9.15 [9.09, 9.23], CV 0.5% | 10.51 [10.40, 10.59], CV 0.7% |
| cache-thrash | 3.81 [3.71, 3.91], CV 1.5% | 9.17 [9.09, 9.25], CV 0.6% | 10.55 [10.41, 10.60], CV 0.6% |
| xmalloc-test | 62.63 [58.19, 67.52], CV 4.0% | 65.99 [65.90, 66.04], CV 0.1% | 127.97 [114.92, 140.60], CV 6.3% |
| malloc-large-old | 3.80 [3.71, 3.86], CV 1.8% | 8.20 [8.07, 8.26], CV 0.8% | 10.45 [10.41, 10.55], CV 0.4% |
| malloc-large | 521.90 [521.81, 522.04], CV 0.0% | 619.14 [617.09, 621.25], CV 0.3% | 447.73 [446.39, 450.45], CV 0.4% |
| mstress | 72.18 [68.93, 78.52], CV 4.8% | 149.22 [134.45, 162.54], CV 7.0% | 67.91 [64.98, 72.98], CV 3.7% |
| mleak | 1.95 [1.94, 1.96], CV 0.3% | 6.33 [6.08, 6.53], CV 2.8% | 21.91 [18.36, 28.60], CV 14.3% |
| rptest | 21.48 [15.55, 27.53], CV 24.4% | 65.93 [60.96, 70.26], CV 3.9% | 77.88 [65.47, 87.80], CV 10.2% |
| glibc-simple | 1.85 [1.84, 1.85], CV 0.1% | 6.38 [6.28, 6.41], CV 0.8% | 14.47 [14.39, 14.59], CV 0.4% |
| glibc-thread | 2.41 [2.41, 2.41], CV 0.1% | 16.44 [16.39, 16.51], CV 0.3% | 26.31 [26.28, 26.32], CV 0.1% |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
