# Memory usage

[Performance](../performance.md) · **English** · [简体中文](memory.zh-CN.md)

**Resident process memory above baseline, MiB.** Subtract each process baseline before taking the median of three fresh processes; counters off.

<!-- performance-source -->
2026-10-11 · [81cfacd](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240) · [CSV](../results/current/README.md)
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
| Standard | Native | 27.89 | 26.26 | 28.89 | 26.64 | 1.46 |
| Standard | UniMemory | 27.89 | 26.26 | 28.89 | 26.40 | 1.46 |
| mimalloc | Native | 31.73 | 31.79 | 46.46 | 46.46 | 46.46 |
| mimalloc | UniMemory | 31.72 | 31.78 | 46.45 | 46.45 | 46.45 |
| jemalloc | Native | 32.23 | 32.29 | 33.48 | 33.48 | 33.48 |
| jemalloc | UniMemory | 32.28 | 32.34 | 33.54 | 33.54 | 33.54 |
<!-- /performance-table -->

### Linux

<!-- performance-table:pressure-linux-x64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 25.67 | 25.86 | 26.50 | 26.50 | 26.50 |
| Standard | UniMemory | 25.67 | 25.86 | 26.50 | 26.50 | 26.50 |
| mimalloc | Native | 34.20 | 34.39 | 50.39 | 50.39 | 50.39 |
| mimalloc | UniMemory | 34.20 | 34.39 | 50.39 | 50.39 | 50.39 |
| jemalloc | Native | 31.20 | 31.32 | 31.32 | 31.32 | 31.32 |
| jemalloc | UniMemory | 31.20 | 31.32 | 31.32 | 31.32 | 31.32 |
<!-- /performance-table -->

### macOS

<!-- performance-table:pressure-macos-arm64 -->
| Backend | Path | Fill | Keep 1024 | Refill | Keep again | Free all |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | Native | 28.61 | 28.61 | 34.44 | 34.44 | 18.44 |
| Standard | UniMemory | 28.61 | 28.61 | 34.44 | 34.44 | 18.44 |
| mimalloc | Native | 33.31 | 33.31 | 48.55 | 48.55 | 48.55 |
| mimalloc | UniMemory | 33.31 | 33.31 | 48.55 | 48.55 | 48.55 |
| jemalloc | Native | 31.78 | 31.78 | 33.11 | 33.11 | 33.11 |
| jemalloc | UniMemory | 31.78 | 31.78 | 33.09 | 33.09 | 33.09 |
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
| Windows | mimalloc | Native | 25.04% |
| Windows | mimalloc | UniMemory | 25.04% |
| Windows | jemalloc | Native | 25.04% |
| Windows | jemalloc | UniMemory | 25.04% |
| Linux | Standard | Native | — |
| Linux | Standard | UniMemory | — |
| Linux | mimalloc | Native | 25.04% |
| Linux | mimalloc | UniMemory | 25.04% |
| Linux | jemalloc | Native | 25.04% |
| Linux | jemalloc | UniMemory | 25.04% |
| macOS | Standard | Native | — |
| macOS | Standard | UniMemory | — |
| macOS | mimalloc | Native | 25.04% |
| macOS | mimalloc | UniMemory | 25.04% |
| macOS | jemalloc | Native | 25.04% |
| macOS | jemalloc | UniMemory | 25.04% |
<!-- /performance-table -->

## Heap collect

16384 × 4096 B: 64 MiB, fully touched. Free each Block, then call collect() on independent Heap.

<!-- performance-table:footprint -->
| Platform | Backend | Kind | Live | Free | Collect |
| --- | --- | --- | --- | --- | --- |
| Windows | Standard | Global | 69.61 | 0.29 | — |
| Windows | mimalloc | Global | 64.31 | 64.32 | — |
| Windows | mimalloc | Heap | 64.29 | 64.29 | 0.30 |
| Windows | jemalloc | Global | 70.08 | 70.08 | — |
| Windows | jemalloc | Heap | 70.09 | 70.09 | 6.12 |
| Linux | Standard | Global | 64.38 | 0.25 | — |
| Linux | mimalloc | Global | 66.14 | 66.14 | — |
| Linux | mimalloc | Heap | 66.13 | 66.13 | 2.13 |
| Linux | jemalloc | Global | 65.12 | 65.12 | — |
| Linux | jemalloc | Heap | 67.12 | 67.12 | 3.25 |
| macOS | Standard | Global | 64.28 | 8.28 | — |
| macOS | mimalloc | Global | 64.34 | 64.34 | — |
| macOS | mimalloc | Heap | 64.30 | 64.30 | 64.30 |
| macOS | jemalloc | Global | 65.53 | 65.53 | — |
| macOS | jemalloc | Heap | 65.55 | 65.55 | 1.55 |
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
