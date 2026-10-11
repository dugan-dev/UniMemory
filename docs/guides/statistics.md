# Statistics

[Index](../README.md) · **English** · [简体中文](statistics.zh-CN.md)

Examples: [Quick Start](../../README.md#quick-start).

## Choose a query

| Query | Scope | Content |
| --- | --- | --- |
| `statistics()` | This Memory | Successful operations, requested bytes, peak |
| `backend_statistics()` | Backend or independent Heap | Available native metrics |
| Stack `used()` | This Buffer | Consumed bytes, including padding and retained storage |

## Counter configuration

`UNIMEMORY_STATISTICS` is required at configuration time. `ON` fixes Global to Basic; `OFF` fixes it to Disabled. `Memory::selected_statistics` exposes that compile-time choice. Global has no runtime reconfiguration function.

For a supported Heap, `Memory::heap(Memory::selected_backend)` defaults to Disabled. Pass `StatisticsMode::Basic` only in an ON build; OFF rejects it with `std::invalid_argument`. Stack never enables counters.

```sh
cmake -S . -B build/basic -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=ON
```

Counters cover successful requests through this Memory, excluding direct native calls. Heap reset clears its counters. Enabling statistics adds counting overhead.

| Field | Meaning |
| --- | --- |
| `allocations` / `deallocations` / `reallocations` | Successful operation counts |
| `live_bytes` | Requested bytes not yet released |
| `peak_live_bytes` | Peak live requested bytes |

## Backend details

Query the instance's `memory.capabilities().detailed_statistics` before using native details. Both statistics queries are empty for Stack; `statistics()` has a value only when Basic counters are enabled.

| Backend / kind | Native scope | Available byte metrics |
| --- | --- | --- |
| Standard Global | — | None |
| mimalloc Global | Process | Committed, reserved |
| mimalloc Heap | — | None; Basic request counters require an ON build |
| jemalloc Global | Process | Allocated, resident; requires `config.stats` |
| jemalloc Heap | Memory | Allocated, resident; requires `config.stats` |

| `scope` | Meaning |
| --- | --- |
| `Memory` | This independent Heap |
| `Process` | The backend's native aggregate scope, which may include direct native allocations |

mimalloc excludes other separately managed native subprocesses. Unavailable fields are empty, not zero. Requested bytes, native metrics and process RSS measure different things and must not be added together.

## When to query

- Use Basic `statistics()` for concurrent monitoring; fields may reflect different instants.
- Query native statistics during diagnostics, rather than on every allocation.
- Before a native query, pause allocation, release and thread cleanup in the relevant Memory/Process scope, including direct native calls.

[Backend metric notes](../allocator-capabilities.md) · [Lifetime and threads](../compatibility.md)
