# Capability comparison

[Index](README.md) · **English** · [简体中文](allocator-capabilities.zh-CN.md)

UniMemory unifies common contracts. Backend capability queries describe optional support; they do not make different physical allocator behavior identical.

| Feature | Standard C++ | mimalloc | jemalloc | UniMemory |
| --- | --- | --- | --- | --- |
| Objects / alignment | new/delete, allocator | Native allocation | Native allocation | Object owners, byte operations and alignment |
| Reallocation | Allocate/copy/free yourself | Native realloc | Native rallocx | One resize contract, preserves old block on failure |
| Zeroed growth | Not a PMR operation | Native form has old-block preconditions | ZERO uses native usable-size boundary | Zero newly requested bytes consistently |
| Containers | allocator / PMR | C++ adapter | Can supply storage | Allocator adapter and PMR resource |
| Request counters | Not required by PMR | Native metrics | Native metrics | Optional Global or Heap Basic counters |
| Independent group | PMR pools have different semantics | Heap | Explicit arena | Memory::heap(), enabled native backends only |
| Group reset / idle collect / membership | No uniform native contract | Heap operations | arena reset/purge/lookup | Heap reset/collect/owns with pointer/lifetime rules |
| Detailed statistics | No allocator internals | Process committed/reserved | stats build + mallctl; Process or Heap | Instance capability, optional fields and explicit scope |
| Reclaim delay | No allocator control | Purge delay | Arena decay defaults | One advisory option |
| Allocation profiling, traversal, OS reservation hooks | No common interface | Native-specific | Native-specific | Not exposed in 0.0.1 |

## Different meanings of arena

| Term | Meaning |
| --- | --- |
| UniMemory Heap | Independently managed growing group; neither fixed nor necessarily contiguous |
| mimalloc heap | Native implementation of Memory::heap(Backend::Mimalloc) |
| mimalloc OS arena | Large reserved OS memory area; a different native facility |
| jemalloc arena | Native allocation group; default arenas are shared, explicit ones back UniMemory Heap |
| Stack Memory | Your fixed buffer with bump allocation and rewind |

PMR unifies storage allocation/free, not every native control or every library's allocation. Using mimalloc does not forbid standard C++ allocation; keep each pointer paired with its correct allocation API. No backend is assumed fastest without a measured workload.

Tested versions: mimalloc **3.4.3**, jemalloc **5.3.1**. Current web manuals can describe newer versions. [Native details](backends/mimalloc.md) · [Build conditions](guides/backends.md) · [Evidence](testing.md)
