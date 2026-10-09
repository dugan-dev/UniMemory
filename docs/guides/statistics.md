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

Default: Disabled. Enable Basic before the first `global()` call, or pass it when creating a Heap. Later mode changes throw `logic_error`; repeating the same mode is allowed.

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
| mimalloc Heap | — | None; Basic request counters remain available |
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
