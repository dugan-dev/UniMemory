# Memory types: Global, Heap and Stack

[Home](../README.md) · [简体中文](heap.zh-CN.md)

Use C++20, include `<unimem/all.h>` and link `UniMemory::UniMemory`. Construct Global, Heap or Stack directly; use `Memory&` for common allocation, object, container and statistics operations. Instances cannot be copied or moved.

| Type | Constructor | Storage and controls |
| --- | --- | --- |
| `Global` | `Global(statistics = Disabled)` | Shared native allocator; caller-owned context |
| `Heap` | `Heap(statistics = Disabled)` | Independent allocation group; `reset`, `collect`, `owns` |
| `Stack` | `Stack(span<byte>, statistics = Disabled)` or `Stack(void*, size_t, statistics = Disabled)` | Borrowed fixed buffer; `mark`, `rewind`, `reset`, `used`, `capacity` |

Each instance has separate request statistics and PMR resource identity. Statistics default to Disabled; Basic requires `UNIMEMORY_STATISTICS=ON`. Pause Memory operations before `set_statistics()` or `reset_statistics()`.

## Global

Global has a public constructor. Multiple instances share the CMake-selected native allocator, but do not share counters or adapter identity. Construction does not replace ordinary `new/delete` or unadapted containers. Destruction does not bulk-free outstanding native allocations: release them through their original Global while it is alive.

```cpp
#include <unimem/all.h>
#include <cassert>

int main() {
    unimem::Global first;
    unimem::Global second;
    assert(first.resource() != second.resource());
    unimem::Memory& memory = first;
    auto value = memory.make_unique<int>(42);
    return *value == 42 ? 0 : 1;
}
```

Pass caller-owned Global by reference across translation units. Across DLL/plugins preserve original Memory/resource identity and keep its defining module and SDK loaded until users are gone. Cross-library C++ objects require compatible compiler, standard-library and runtime ABIs.

## Heap

All backends support independent Heap: a native mimalloc heap, dedicated jemalloc arena, or Standard allocation group. Physical page isolation is not guaranteed.

| Operation | Behavior |
| --- | --- |
| `reset()` | Release all blocks and clear request counters; failure preserves allocations |
| `collect()` | Attempt idle-memory reclamation while preserving live allocations |
| `owns(ptr)` | Check ownership of a live allocation-start pointer; null is false |

`owns()` is not a pointer validator: other pointers must be live allocation starts from the same backend, never arbitrary or dangling addresses. Collection need not immediately reduce memory usage or drain every thread cache.

```cpp
#include <unimem/all.h>
#include <cassert>

int main() {
    unimem::Heap heap;
    {
        auto block = heap.make_block(64);
        assert(heap.owns(block.data()));
        heap.collect(); // live block stays valid
    }
    heap.reset(); // consumers are gone
}
```

Destroy objects, containers, owners and remaining weak-pointer control blocks before reset/destruction. Bulk release does not run C++ destructors.

## Stack

Stack uses a **borrowed writable fixed-capacity buffer**, not the thread call stack. The buffer must outlive Stack and its users. Use one thread only and no active overlapping buffer allocators. Capacity exhaustion throws `std::bad_alloc` without changing the offset; there is no Heap fallback. Invalid buffer configuration throws `std::invalid_argument`.

| Operation | Behavior |
| --- | --- |
| `allocate()` | Advance offset, including alignment padding |
| `deallocate()` / owner destruction | Retain buffer space; owner destruction still runs object destructors |
| `mark()` | Save offset as opaque `Stack::Mark` |
| `rewind(mark)` | Return to saved offset and invalidate all old marks |
| `reset()` | Return to zero, clear counters and invalidate all old marks |
| `used()` / `capacity()` | Consumed buffer bytes / total bytes |

Destroy affected objects and users before rewind/reset, which do not run destructors. Foreign/stale marks throw `std::invalid_argument`; marks must not outlive Stack. Rewinding an inner mark invalidates outer marks too, so old marks cannot be unwound one level at a time.

```cpp
#include <unimem/all.h>
#include <array>
#include <cassert>
#include <cstddef>
#include <span>

int main() {
    alignas(std::max_align_t) std::array<std::byte, 1024> buffer{};
    unimem::Stack scratch{std::span<std::byte>{buffer}};
    auto checkpoint = scratch.mark();
    {
        auto value = scratch.make_unique<int>(42);
        assert(*value == 42);
    } // destroy object, retain buffer space
    assert(scratch.used() > 0);
    scratch.rewind(checkpoint);
    assert(scratch.used() == 0);
}
```

Growth extends in place when the block ends at the current offset and fits; otherwise it allocates/copies within the same buffer. Shrinking/freeing retain occupied space. Failed resize preserves the old pointer, contents and offset. Failed object construction also retains consumed space. `used()` includes padding and retained storage; request counters measure a different quantity. Stack has no native statistics.

## Threads and configuration

Global and Heap permit concurrent allocation/free. Synchronize object/container/block access and ownership publication separately. Heap collect/reset and all Memory destruction require exclusive access: no concurrent operations or access to reclaimed storage. Stack is single-threaded. Every Memory must outlive its owners, containers, adapters and shared/weak control blocks.

CMake selects `standard`, `mimalloc` or `jemalloc` once per profile. No runtime backend registration or allocator plugin ABI exists. `Memory::config(release_delay_ms)` sets backend-wide idle-page release policy; `-1` disables automatic reclamation. Configure before starting workers. A new backend needs source changes and an explicit dependency decision. Keep matching headers, generated configuration and libraries across translation units; UniMemory 0.x does not promise stable ABI.
