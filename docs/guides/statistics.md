# Statistics

[Index](../README.md) · **English** · [简体中文](statistics.zh-CN.md)

## Choose a query

| Query | Scope | Content |
| --- | --- | --- |
| `statistics()` | This Memory | Successful operations, requested bytes, peak |
| `backend_statistics()` | Backend or independent Heap | Available native metrics |
| Stack `used()` | This Buffer | Consumed bytes, including padding and retained storage |

## Enable request counters

```cpp
unimem::Memory::configure_global(unimem::Backend::Mimalloc,
    unimem::StatisticsMode::Basic);
unimem::Memory& memory = unimem::Memory::global(unimem::Backend::Mimalloc);
unimem::Memory heap = unimem::Memory::heap(unimem::Backend::Mimalloc,
    unimem::StatisticsMode::Basic);
```

Configure before the first Global lookup. Changing mode after initialization throws `logic_error`; identical repeats are allowed. Default: Disabled.

Global aggregates requests from all its users. Heap has independent counters; reset starts a new epoch. Direct native calls are excluded. Disabled adds no statistics atomics; Basic updates atomics on successful operations. Concurrent fields may reflect different instants.

| Field | Meaning |
| --- | --- |
| `allocations` / `deallocations` / `reallocations` | Successful operation counts |
| `live_bytes` | Requested bytes not yet logically released |
| `peak_live_bytes` | Peak live requested bytes |

## Backend details

Query the instance's `memory.capabilities().detailed_statistics` before using native details. Both statistics queries are empty for Stack; `statistics()` has a value only when Basic counters are enabled.

| Backend / kind | Native scope | Available byte metrics |
| --- | --- | --- |
| Standard Global | — | None |
| mimalloc Global | Process | Committed, reserved |
| mimalloc Heap | — | None; Basic request counters remain available |
| jemalloc Global | Process | Allocated, resident; requires `config.stats` |
| jemalloc Heap | Memory | Allocated, resident; requires `config.stats` |

`BackendStatisticsScope::Process` denotes the native Backend's aggregate scope; `Memory` covers this independent Heap. mimalloc's aggregate covers the calling thread's current native subprocess, normally main; separately managed native subprocesses are excluded. Unavailable fields are empty optionals. Process RSS differs from requested bytes; do not add metrics with different scopes. Native snapshots may synchronize and belong outside allocation hot paths.

For this diagnostic query, pause the relevant allocation, release and thread
cleanup: this Heap for Memory scope, or this Backend for Process scope, including
native calls outside UniMemory. The unified interface does not guarantee a safe
concurrent native snapshot. Use Basic `statistics()` for concurrent monitoring.

mimalloc 3.4.3's malloc counters depend on build settings and do not consistently
track current live bytes; Heap page metrics are recorded at subprocess scope.
These unavailable metrics return empty optionals, never a fabricated zero.
Use Basic counters for requested bytes. [Verified mapping](../backends/mimalloc.md)

mimalloc 3.4.3 copies its native statistics without a lock covering the whole
snapshot ([source](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/stats.c#L536)).
This conservative shared contract does not imply that jemalloc's native statistics
lack synchronization.

```cpp
std::optional<unimem::MemoryStatistics> requests = memory.statistics();
std::optional<unimem::BackendStatistics> details = memory.backend_statistics();
```
