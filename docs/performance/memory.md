# Memory usage

[Performance](../performance.md) · **English** · [简体中文](memory.zh-CN.md)

**Resident process memory above baseline, MiB.** Subtract each process baseline before aggregation; counters off.

<!-- performance-source -->
2026-10-11 · [5953761](https://github.com/dugan-dev/UniMemory/actions/runs/38122935359) · [CSV](../results/current/README.md)

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
| Standard | Native | 27.89 [27.89, 27.90], CV 0.0% | 26.25 [26.24, 26.26], CV 0.0% | 28.88 [28.87, 28.90], CV 0.0% | 26.52 [26.38, 26.66], CV 0.5% | 1.45 [1.45, 1.46], CV 0.2% |
| Standard | UniMemory | 27.89 [27.86, 27.91], CV 0.0% | 26.25 [26.22, 26.27], CV 0.1% | 28.88 [28.85, 28.91], CV 0.1% | 26.40 [25.89, 26.66], CV 0.8% | 1.46 [1.44, 1.47], CV 0.6% |
| mimalloc | Native | 31.72 [31.72, 31.73], CV 0.0% | 31.77 [31.77, 31.77], CV 0.0% | 46.44 [46.44, 46.44], CV 0.0% | 46.44 [46.44, 46.44], CV 0.0% | 46.44 [46.44, 46.44], CV 0.0% |
| mimalloc | UniMemory | 31.72 [31.71, 31.72], CV 0.0% | 31.76 [31.76, 31.77], CV 0.0% | 46.43 [46.43, 46.44], CV 0.0% | 46.43 [46.43, 46.44], CV 0.0% | 46.43 [46.43, 46.44], CV 0.0% |
| jemalloc | Native | 32.24 [32.21, 32.27], CV 0.1% | 32.29 [32.26, 32.32], CV 0.1% | 33.49 [33.46, 33.52], CV 0.1% | 33.49 [33.46, 33.52], CV 0.1% | 33.49 [33.46, 33.52], CV 0.1% |
| jemalloc | UniMemory | 32.28 [32.26, 32.31], CV 0.1% | 32.33 [32.30, 32.36], CV 0.1% | 33.52 [33.50, 33.55], CV 0.0% | 33.52 [33.50, 33.55], CV 0.0% | 33.52 [33.50, 33.55], CV 0.0% |
<!-- /performance-table -->

### Linux

<!-- performance-table:pressure-linux-x64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 25.67 [25.67, 25.67], CV 0.0% | 25.86 [25.86, 25.86], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% |
| Standard | UniMemory | 25.67 [25.67, 25.67], CV 0.0% | 25.86 [25.86, 25.86], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% | 26.50 [26.50, 26.50], CV 0.0% |
| mimalloc | Native | 34.20 [34.20, 34.20], CV 0.0% | 34.39 [34.39, 34.39], CV 0.0% | 50.44 [50.39, 50.95], CV 0.4% | 50.45 [50.39, 50.95], CV 0.4% | 50.50 [50.39, 50.95], CV 0.5% |
| mimalloc | UniMemory | 34.20 [34.20, 34.20], CV 0.0% | 34.39 [34.39, 34.39], CV 0.0% | 50.45 [50.39, 50.96], CV 0.4% | 50.45 [50.39, 50.96], CV 0.4% | 50.45 [50.39, 50.96], CV 0.4% |
| jemalloc | Native | 31.20 [31.20, 31.20], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% |
| jemalloc | UniMemory | 31.20 [31.20, 31.20], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% | 31.32 [31.32, 31.32], CV 0.0% |
<!-- /performance-table -->

### macOS

<!-- performance-table:pressure-macos-arm64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 28.63 [28.62, 28.67], CV 0.1% | 28.63 [28.62, 28.69], CV 0.1% | 37.58 [34.45, 43.52], CV 11.4% | 37.58 [34.45, 43.52], CV 11.4% | 21.58 [18.45, 27.52], CV 19.9% |
| Standard | UniMemory | 28.61 [28.61, 28.61], CV 0.0% | 28.61 [28.61, 28.61], CV 0.0% | 41.68 [34.44, 43.50], CV 9.2% | 41.68 [34.44, 43.50], CV 9.2% | 28.88 [18.44, 35.50], CV 23.1% |
| mimalloc | Native | 33.30 [33.30, 33.33], CV 0.0% | 33.32 [33.31, 33.36], CV 0.1% | 48.56 [48.55, 48.59], CV 0.0% | 48.56 [48.55, 48.59], CV 0.0% | 48.56 [48.55, 48.59], CV 0.0% |
| mimalloc | UniMemory | 33.30 [33.30, 33.31], CV 0.0% | 33.30 [33.30, 33.31], CV 0.0% | 48.54 [48.53, 48.55], CV 0.0% | 48.54 [48.53, 48.55], CV 0.0% | 48.54 [48.53, 48.55], CV 0.0% |
| jemalloc | Native | 31.79 [31.78, 31.80], CV 0.0% | 31.81 [31.80, 31.81], CV 0.0% | 33.13 [33.12, 33.14], CV 0.0% | 33.13 [33.12, 33.14], CV 0.0% | 33.13 [33.12, 33.14], CV 0.0% |
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
| Windows | Standard | Global | 69.61 [69.61, 69.61], CV 0.0% | 0.29 [0.29, 0.29], CV 1.2% | — |
| Windows | mimalloc | Global | 64.31 [64.31, 64.31], CV 0.0% | 64.32 [64.32, 64.32], CV 0.0% | — |
| Windows | mimalloc | Heap | 64.29 [64.29, 64.29], CV 0.0% | 64.29 [64.29, 64.29], CV 0.0% | 0.29 [0.29, 0.29], CV 0.0% |
| Windows | jemalloc | Global | 70.06 [70.04, 70.08], CV 0.0% | 70.07 [70.04, 70.09], CV 0.0% | — |
| Windows | jemalloc | Heap | 70.10 [70.07, 70.12], CV 0.0% | 70.11 [70.08, 70.12], CV 0.0% | 6.14 [6.11, 6.15], CV 0.2% |
| Linux | Standard | Global | 64.38 [64.38, 64.38], CV 0.0% | 0.25 [0.25, 0.25], CV 0.0% | — |
| Linux | mimalloc | Global | 66.14 [66.14, 66.14], CV 0.0% | 66.14 [66.14, 66.14], CV 0.0% | — |
| Linux | mimalloc | Heap | 66.13 [66.13, 66.14], CV 0.0% | 66.13 [66.13, 66.14], CV 0.0% | 2.13 [2.13, 2.14], CV 0.1% |
| Linux | jemalloc | Global | 65.53 [65.12, 69.12], CV 1.9% | 65.53 [65.12, 69.12], CV 1.9% | — |
| Linux | jemalloc | Heap | 67.12 [67.12, 67.12], CV 0.0% | 67.12 [67.12, 67.12], CV 0.0% | 3.25 [3.25, 3.25], CV 0.0% |
| macOS | Standard | Global | 64.28 [64.28, 64.30], CV 0.0% | 10.68 [8.28, 24.30], CV 50.6% | — |
| macOS | mimalloc | Global | 64.34 [64.33, 64.34], CV 0.0% | 64.34 [64.33, 64.34], CV 0.0% | — |
| macOS | mimalloc | Heap | 64.31 [64.30, 64.31], CV 0.0% | 64.31 [64.30, 64.31], CV 0.0% | 64.31 [64.30, 64.31], CV 0.0% |
| macOS | jemalloc | Global | 65.54 [65.53, 65.55], CV 0.0% | 65.54 [65.53, 65.55], CV 0.0% | — |
| macOS | jemalloc | Heap | 65.54 [65.53, 65.55], CV 0.0% | 65.54 [65.53, 65.55], CV 0.0% | 1.54 [1.53, 1.55], CV 0.5% |
<!-- /performance-table -->

collect() reclaims eligible idle resources; it does not destroy live objects or guarantee an immediate memory decrease.

## Library storage

<!-- performance-table:storage -->
| Platform | Memory / B | OwnedBlock / B | `Allocator<T>` / B |
| --- | --- | --- | --- |
| Windows | 80 | 32 | 8 |
| Linux | 80 | 32 | 8 |
| macOS | 80 | 32 | 8 |
<!-- /performance-table -->

Type sizes come from the measured builds. They exclude Global shared state, Basic counters and native metadata. Stack setup requires no separate state allocation.

Linux uses smaps_rollup RSS, Windows uses the working set, and macOS uses resident size. OS peak counters differ from phase snapshots.

[Method](../benchmarking.md) · [Raw data](../results/current/README.md) · [Native programs](applications.md)
