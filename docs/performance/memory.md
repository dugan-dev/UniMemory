# Memory usage

[Performance](../performance.md) · **English** · [简体中文](memory.zh-CN.md)

**Resident process memory above baseline, MiB.** Subtract each process baseline before aggregation; counters off.

<!-- performance-source -->
2026-10-13 · [c917e81](https://github.com/dugan-dev/UniMemory/actions/runs/38415666741) · [CSV](../results/current/README.md)

10 fresh processes; mean [min, max], sample CV. All trials retained; sample SD and bootstrap 95% CI in raw summary.json.
<!-- /performance-source -->

Requested bytes are storage the application still needs. Resident memory also includes allocator caches and metadata. Freed storage can remain available for reuse.

## Mixed sizes and lifetimes

16384 Blocks, ten sizes from 17–8193 B, eight refill/free cycles; keep 1024 Blocks. All requested pages are touched.

Snapshots immediately follow operations, without an intentional wait or collection. Columns show initial fill, first survivors, final refill, final survivors and free all.

Live requested storage, MiB; the same workload runs on all platforms:

<!-- performance-table:pressure-requested -->
| Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- |
| 25.18 | 1.61 | 25.81 | 1.61 | 0.00 |
<!-- /performance-table -->

### Windows

<!-- performance-table:pressure-windows-x64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 26.50 [26.49, 26.52], CV 0.0% | 24.88 [24.86, 24.90], CV 0.0% | 27.42 [27.40, 27.45], CV 0.1% | 25.28 [25.22, 25.43], CV 0.3% | 1.49 [1.47, 1.51], CV 0.8% |
| Standard | UniMemory | 26.50 [26.49, 26.51], CV 0.0% | 24.87 [24.86, 24.88], CV 0.0% | 27.41 [27.39, 27.41], CV 0.0% | 25.31 [25.22, 25.40], CV 0.3% | 1.49 [1.47, 1.52], CV 1.0% |
| mimalloc | Native | 31.73 [31.73, 31.73], CV 0.0% | 31.77 [31.77, 31.78], CV 0.0% | 46.44 [46.44, 46.45], CV 0.0% | 46.44 [46.44, 46.45], CV 0.0% | 46.44 [46.44, 46.45], CV 0.0% |
| mimalloc | UniMemory | 31.72 [31.72, 31.73], CV 0.0% | 31.76 [31.76, 31.77], CV 0.0% | 46.43 [46.43, 46.44], CV 0.0% | 46.43 [46.43, 46.44], CV 0.0% | 46.43 [46.43, 46.44], CV 0.0% |
| jemalloc | Native | 32.24 [32.22, 32.25], CV 0.0% | 32.29 [32.27, 32.30], CV 0.0% | 33.48 [33.46, 33.50], CV 0.0% | 33.48 [33.46, 33.50], CV 0.0% | 33.48 [33.46, 33.50], CV 0.0% |
| jemalloc | UniMemory | 32.28 [32.25, 32.30], CV 0.1% | 32.33 [32.30, 32.36], CV 0.1% | 33.52 [33.50, 33.55], CV 0.0% | 33.52 [33.50, 33.55], CV 0.0% | 33.52 [33.50, 33.55], CV 0.0% |
<!-- /performance-table -->

### Linux

<!-- performance-table:pressure-linux-x64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 25.67 [25.67, 25.67], CV 0.0% | 25.86 [25.86, 25.86], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% |
| Standard | UniMemory | 25.67 [25.67, 25.67], CV 0.0% | 25.86 [25.86, 25.86], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% |
| mimalloc | Native | 34.20 [34.20, 34.20], CV 0.0% | 34.39 [34.39, 34.39], CV 0.0% | 50.43 [50.39, 50.83], CV 0.3% | 50.45 [50.39, 50.96], CV 0.4% | 50.45 [50.39, 50.96], CV 0.4% |
| mimalloc | UniMemory | 34.20 [34.20, 34.20], CV 0.0% | 34.39 [34.39, 34.39], CV 0.0% | 50.39 [50.39, 50.39], CV 0.0% | 50.39 [50.39, 50.45], CV 0.0% | 50.44 [50.39, 50.89], CV 0.3% |
| jemalloc | Native | 31.20 [31.20, 31.20], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% |
| jemalloc | UniMemory | 31.60 [31.20, 35.20], CV 4.0% | 31.72 [31.32, 35.32], CV 4.0% | 31.72 [31.32, 35.32], CV 4.0% | 31.72 [31.32, 35.32], CV 4.0% | 31.72 [31.32, 35.32], CV 4.0% |
<!-- /performance-table -->

### macOS

<!-- performance-table:pressure-macos-arm64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 28.63 [28.62, 28.67], CV 0.1% | 28.63 [28.62, 28.67], CV 0.1% | 40.21 [34.45, 43.52], CV 10.8% | 40.21 [34.45, 43.52], CV 10.8% | 29.01 [18.45, 35.52], CV 26.9% |
| Standard | UniMemory | 28.61 [28.61, 28.64], CV 0.0% | 28.61 [28.61, 28.66], CV 0.1% | 40.25 [34.44, 46.77], CV 11.4% | 40.25 [34.44, 46.77], CV 11.4% | 25.85 [18.44, 35.50], CV 23.2% |
| mimalloc | Native | 33.32 [33.31, 33.33], CV 0.0% | 33.34 [33.33, 33.39], CV 0.1% | 48.58 [48.56, 48.62], CV 0.0% | 48.58 [48.56, 48.62], CV 0.0% | 48.58 [48.56, 48.62], CV 0.0% |
| mimalloc | UniMemory | 33.32 [33.31, 33.33], CV 0.0% | 33.33 [33.31, 33.38], CV 0.1% | 48.56 [48.55, 48.61], CV 0.0% | 48.56 [48.55, 48.61], CV 0.0% | 48.56 [48.55, 48.61], CV 0.0% |
| jemalloc | Native | 31.80 [31.78, 31.80], CV 0.0% | 31.81 [31.80, 31.81], CV 0.0% | 33.13 [33.12, 33.14], CV 0.0% | 33.13 [33.12, 33.14], CV 0.0% | 33.13 [33.12, 33.14], CV 0.0% |
| jemalloc | UniMemory | 31.78 [31.77, 31.78], CV 0.0% | 31.78 [31.77, 31.78], CV 0.0% | 33.10 [33.09, 33.11], CV 0.0% | 33.10 [33.09, 33.11], CV 0.0% | 33.10 [33.09, 33.11], CV 0.0% |
<!-- /performance-table -->

![Windows mixed-lifetime memory](../images/memory-retention.png)

![Linux mixed-lifetime memory](../images/performance/linux-x64/retention.svg)

![macOS mixed-lifetime memory](../images/performance/macos-arm64/retention.svg)

Charts connect phase snapshots, not continuous samples. Freed storage can remain resident; retention alone is not a leak or fragmentation rate.

## Size rounding

Extra usable capacity relative to requested bytes at initial fill. Standard has no portable capacity query. This excludes metadata and caches.

<!-- performance-table:rounding -->
| Platform | Backend | Path | Extra usable capacity |
| --- | --- | --- | --- |
| Windows | Standard | Native | — |
| Windows | Standard | UniMemory | — |
| Windows | mimalloc | Native | 25.04% [25.04, 25.04], CV 0.0% |
| Windows | mimalloc | UniMemory | 25.04% [25.04, 25.04], CV 0.0% |
| Windows | jemalloc | Native | 25.04% [25.04, 25.04], CV 0.0% |
| Windows | jemalloc | UniMemory | 25.04% [25.04, 25.04], CV 0.0% |
| Linux | Standard | Native | — |
| Linux | Standard | UniMemory | — |
| Linux | mimalloc | Native | 25.04% [25.04, 25.04], CV 0.0% |
| Linux | mimalloc | UniMemory | 25.04% [25.04, 25.04], CV 0.0% |
| Linux | jemalloc | Native | 25.04% [25.04, 25.04], CV 0.0% |
| Linux | jemalloc | UniMemory | 25.04% [25.04, 25.04], CV 0.0% |
| macOS | Standard | Native | — |
| macOS | Standard | UniMemory | — |
| macOS | mimalloc | Native | 25.04% [25.04, 25.04], CV 0.0% |
| macOS | mimalloc | UniMemory | 25.04% [25.04, 25.04], CV 0.0% |
| macOS | jemalloc | Native | 25.04% [25.04, 25.04], CV 0.0% |
| macOS | jemalloc | UniMemory | 25.04% [25.04, 25.04], CV 0.0% |
<!-- /performance-table -->

## Heap collect

16384 × 4096 B: 64 MiB, fully touched. Free each Block, then call collect() on independent Heap.

<!-- performance-table:footprint -->
| Platform | Backend | Kind | Live | Free | Collect |
| --- | --- | --- | --- | --- | --- |
| Windows | Standard | Global | 65.32 [65.32, 65.32], CV 0.0% | 0.94 [0.81, 1.07], CV 14.4% | — |
| Windows | Standard | Heap | 66.27 [66.27, 66.27], CV 0.0% | 0.52 [0.51, 0.52], CV 0.8% | 0.52 [0.51, 0.52], CV 0.8% |
| Windows | mimalloc | Global | 64.31 [64.31, 64.31], CV 0.0% | 64.32 [64.32, 64.32], CV 0.0% | — |
| Windows | mimalloc | Heap | 64.29 [64.29, 64.30], CV 0.0% | 64.29 [64.29, 64.30], CV 0.0% | 0.30 [0.30, 0.30], CV 0.4% |
| Windows | jemalloc | Global | 70.06 [70.04, 70.08], CV 0.0% | 70.06 [70.04, 70.09], CV 0.0% | — |
| Windows | jemalloc | Heap | 70.09 [70.07, 70.12], CV 0.0% | 70.10 [70.07, 70.12], CV 0.0% | 6.13 [6.10, 6.16], CV 0.4% |
| Linux | Standard | Global | 64.38 [64.38, 64.38], CV 0.0% | 0.25 [0.25, 0.25], CV 0.0% | — |
| Linux | Standard | Heap | 65.29 [65.29, 65.29], CV 0.0% | 65.29 [65.29, 65.29], CV 0.0% | 4.84 [4.84, 4.84], CV 0.0% |
| Linux | mimalloc | Global | 66.14 [66.14, 66.14], CV 0.0% | 66.14 [66.14, 66.14], CV 0.0% | — |
| Linux | mimalloc | Heap | 66.13 [66.13, 66.14], CV 0.0% | 66.13 [66.13, 66.14], CV 0.0% | 2.13 [2.13, 2.14], CV 0.1% |
| Linux | jemalloc | Global | 65.72 [65.12, 69.12], CV 2.1% | 65.72 [65.12, 69.12], CV 2.1% | — |
| Linux | jemalloc | Heap | 67.53 [67.12, 71.12], CV 1.9% | 67.53 [67.12, 71.12], CV 1.9% | 3.65 [3.25, 7.25], CV 34.7% |
| macOS | Standard | Global | 64.28 [64.28, 64.30], CV 0.0% | 13.88 [8.28, 32.30], CV 54.7% | — |
| macOS | Standard | Heap | 65.49 [65.47, 65.52], CV 0.0% | 13.49 [9.47, 25.52], CV 42.1% | 13.49 [9.47, 25.52], CV 42.1% |
| macOS | mimalloc | Global | 64.33 [64.33, 64.34], CV 0.0% | 64.33 [64.33, 64.34], CV 0.0% | — |
| macOS | mimalloc | Heap | 64.30 [64.30, 64.31], CV 0.0% | 64.30 [64.30, 64.31], CV 0.0% | 64.30 [64.30, 64.31], CV 0.0% |
| macOS | jemalloc | Global | 65.54 [65.53, 65.55], CV 0.0% | 65.54 [65.53, 65.55], CV 0.0% | — |
| macOS | jemalloc | Heap | 65.54 [65.53, 65.55], CV 0.0% | 65.54 [65.53, 65.55], CV 0.0% | 1.54 [1.53, 1.55], CV 0.5% |
<!-- /performance-table -->

collect() reclaims eligible idle resources; it does not destroy live objects or guarantee an immediate memory decrease.

## Library storage

<!-- performance-table:storage -->
| Platform | Memory / B | OwnedBlock / B | `Allocator<T>` / B |
| --- | --- | --- | --- |
| Windows | 48 | 32 | 8 |
| Linux | 48 | 32 | 8 |
| macOS | 48 | 32 | 8 |
<!-- /performance-table -->

Type sizes come from the measured builds. They exclude Global shared state, Basic counters and native metadata. Stack setup requires no separate state allocation.

Linux uses smaps_rollup RSS, Windows uses the working set, and macOS uses resident size. OS peak counters differ from phase snapshots.

[Method](../benchmarking.md) · [Raw data](../results/current/README.md) · [Native programs](applications.md)
