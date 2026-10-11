# Native programs

[Performance](../performance.md) · **English** · [简体中文](applications.zh-CN.md)

Separate Linux programs from mimalloc-bench, using process-wide allocator replacement. **These compare native allocators, not UniMemory wrapper cost.**

<!-- performance-source -->
2026-10-11 · [5953761](https://github.com/dugan-dev/UniMemory/actions/runs/38122935359) · [CSV](../results/current/native-applications/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 programs × 3 allocators. The system allocator is glibc; threaded workloads use four workers. Trial counts and statistics are recorded above.

## Elapsed time

**Seconds, lower is better.** Larson, larson-sized and xmalloc-test run for a configured duration, so elapsed time is omitted from comparisons; it remains in the CSV.

<!-- performance-table:applications-elapsed -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 4.601 [4.574, 4.627], CV 0.6% | 4.196 [4.174, 4.276], CV 0.8% | 4.316 [4.275, 4.378], CV 0.7% |
| espresso | 5.003 [4.977, 5.028], CV 0.5% | 4.447 [4.424, 4.479], CV 0.6% | 4.561 [4.524, 4.578], CV 0.5% |
| barnes | 3.358 [3.273, 3.424], CV 1.4% | 3.359 [3.323, 3.475], CV 1.4% | 3.354 [3.322, 3.424], CV 1.1% |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 8.240 [7.997, 8.606], CV 2.1% | 6.386 [6.240, 6.645], CV 2.3% | 6.417 [6.338, 6.593], CV 1.3% |
| cache-scratch | 0.672 [0.668, 0.676], CV 0.3% | 0.700 [0.670, 0.722], CV 3.6% | 0.671 [0.669, 0.675], CV 0.3% |
| cache-thrash | 0.670 [0.668, 0.672], CV 0.2% | 0.716 [0.677, 0.723], CV 1.9% | 0.671 [0.669, 0.673], CV 0.3% |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 [0.004, 0.004], CV 1.1% | 0.008 [0.005, 0.017], CV 39.5% | 0.008 [0.008, 0.008], CV 0.4% |
| malloc-large | 1.955 [1.819, 2.373], CV 8.4% | 1.765 [1.671, 1.921], CV 4.1% | 4.201 [3.974, 4.528], CV 3.7% |
| mstress | 0.333 [0.315, 0.371], CV 7.5% | 0.304 [0.268, 0.321], CV 7.7% | 0.520 [0.518, 0.523], CV 0.3% |
| mleak | 0.064 [0.064, 0.064], CV 0.2% | 0.075 [0.064, 0.116], CV 28.2% | 0.064 [0.064, 0.064], CV 0.2% |
| rptest | 16.052 [16.044, 16.058], CV 0.0% | 16.047 [16.010, 16.060], CV 0.1% | 16.056 [16.048, 16.059], CV 0.0% |
| glibc-simple | 2.941 [2.920, 2.972], CV 0.9% | 2.080 [2.069, 2.120], CV 1.0% | 2.535 [2.519, 2.571], CV 1.0% |
| glibc-thread | 2.026 [2.023, 2.030], CV 0.1% | 2.027 [2.024, 2.028], CV 0.1% | 2.027 [2.023, 2.030], CV 0.1% |
<!-- /performance-table -->

## Peak memory

**Peak RSS, MiB, lower is better.**

<!-- performance-table:applications-memory -->
| Program | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.16 [3.06, 3.26], CV 2.4% | 7.07 [7.03, 7.13], CV 0.6% | 10.60 [10.54, 10.70], CV 0.6% |
| espresso | 2.57 [2.56, 2.57], CV 0.1% | 12.61 [12.50, 12.63], CV 0.3% | 10.64 [10.48, 10.75], CV 1.0% |
| barnes | 60.56 [60.46, 60.61], CV 0.1% | 67.14 [67.09, 67.18], CV 0.0% | 74.29 [73.02, 75.20], CV 1.4% |
| larson | 40.22 [37.19, 44.98], CV 5.7% | 62.40 [60.19, 66.09], CV 3.1% | 64.35 [59.18, 67.86], CV 4.4% |
| larson-sized | 39.69 [37.57, 40.70], CV 2.5% | 63.00 [58.06, 66.21], CV 3.4% | 65.15 [60.00, 67.18], CV 4.2% |
| alloc-test | 14.76 [14.68, 14.85], CV 0.5% | 21.33 [21.23, 21.44], CV 0.3% | 27.91 [27.29, 29.36], CV 3.3% |
| cache-scratch | 3.83 [3.71, 3.91], CV 1.7% | 9.18 [8.99, 9.25], CV 0.9% | 10.55 [10.40, 10.60], CV 0.6% |
| cache-thrash | 3.84 [3.77, 3.92], CV 1.6% | 9.17 [9.09, 9.25], CV 0.6% | 10.57 [10.56, 10.59], CV 0.1% |
| xmalloc-test | 62.78 [58.05, 68.35], CV 4.5% | 75.59 [65.92, 98.04], CV 20.5% | 126.22 [115.13, 143.15], CV 7.0% |
| malloc-large-old | 3.82 [3.71, 3.87], CV 1.6% | 8.21 [8.06, 8.26], CV 0.8% | 10.43 [10.39, 10.45], CV 0.2% |
| malloc-large | 521.90 [521.84, 522.02], CV 0.0% | 619.53 [617.07, 621.20], CV 0.3% | 448.23 [446.42, 450.45], CV 0.4% |
| mstress | 74.20 [67.84, 79.62], CV 6.8% | 137.40 [112.32, 158.33], CV 12.4% | 68.15 [65.25, 72.32], CV 3.6% |
| mleak | 1.94 [1.85, 2.07], CV 2.9% | 6.25 [6.03, 6.41], CV 2.7% | 19.91 [18.44, 22.62], CV 9.5% |
| rptest | 21.07 [16.04, 26.65], CV 19.0% | 64.84 [59.11, 68.95], CV 4.9% | 77.62 [60.41, 93.61], CV 13.9% |
| glibc-simple | 1.85 [1.84, 1.91], CV 1.1% | 6.38 [6.27, 6.40], CV 0.7% | 14.43 [14.24, 14.55], CV 0.5% |
| glibc-thread | 2.40 [2.35, 2.47], CV 1.4% | 16.45 [16.44, 16.51], CV 0.1% | 26.51 [26.29, 28.26], CV 2.3% |
<!-- /performance-table -->

## Interpretation

Allocator results vary across workloads; faster need not mean less memory. Choose using the object sizes, threads and lifetimes of your application.

Elapsed time uses a monotonic clock and includes process startup. Peak RSS comes from GNU time and differs from phase snapshots in the memory report.

[All raw runs](../results/current/native-applications/README.md) · [Reproduce](../benchmarking.md)
