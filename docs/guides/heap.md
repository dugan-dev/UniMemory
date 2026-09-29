# Independent Heap

[Index](../README.md) · **English** · [简体中文](heap.zh-CN.md)

Examples: [Quick Start](../../README.md#heap).

## Scope and lifetime

`Memory::heap(backend)` creates an independent allocation group that grows as needed. Allocations need not be contiguous. All common Memory APIs are available; check `capabilities(backend).heap` first. Standard does not support Heap.

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
