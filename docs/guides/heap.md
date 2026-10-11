# Independent Heap

[Index](../README.md) · **English** · [简体中文](heap.zh-CN.md)

Examples: [Quick Start](../../README.md#quick-start).

## Scope and lifetime

`Memory::heap()` creates an independent allocation group using the CMake-selected backend. It grows as needed; allocations need not be contiguous. All common Memory APIs are available; check `capabilities().heap` first. Standard does not support Heap. The default Heap statistics mode is Disabled. `Memory::heap(StatisticsMode::Basic)` is allowed only when `UNIMEMORY_STATISTICS=ON`, and throws `std::invalid_argument` in an OFF build.

## Operations

| Method | Purpose |
| --- | --- |
| `reset()` | Release all blocks and restart Basic counters; failure preserves allocations |
| `collect()` | Reclaim idle memory while preserving live allocations |
| `owns(ptr)` | Check whether a live allocation belongs to this Heap |
| `statistics()` | Optional request counters for this Heap |
| `backend_statistics()` | Optional native metrics; jemalloc requires `config.stats` |

## Usage rules

- Destroy objects, containers, owners and remaining weak_ptr control blocks before reset/destruction; bulk release does not run object destructors
- Allocation/free may run concurrently; collect, reset and Memory destruction require exclusive access; synchronize object access separately
- `owns(nullptr)` returns false; other pointers must be live allocation starts from the same backend, not arbitrary or dangling addresses
- Collection need not immediately reduce memory usage or drain every thread cache

```mermaid
flowchart LR
    A[Create Heap] --> B[Use objects / containers / blocks]
    B --> C[Destroy objects and owners]
    C --> D[Reset or destroy Heap]
```

[Stack](stack.md) · [Statistics](statistics.md) · [Lifetime and threads](../compatibility.md)
