# API reference

[Index](README.md) · **English** · [简体中文](api-reference.zh-CN.md)

Namespace: `unimem`. Include `<unimem/all.h>` for the complete header-only allocation API; link the CMake `UniMemory::UniMemory` interface target. `Memory` is a non-template abstract base with a virtual destructor. `Global`, `Heap` and `Stack` are final subclasses; allocation, ownership, Allocator/PMR and statistics remain available through `Memory&`.

## 1 · Create Memory

| Signature | Result / contract |
| --- | --- |
| `Memory::global(statistics = Disabled)` | `Global&`; the unique process-lifetime singleton using the CMake-selected backend |
| `Heap(statistics = Disabled)` / `Memory::heap(statistics = Disabled)` | Independent `Heap`, supported by all backends |
| `Stack(span<byte>, statistics = Disabled)` / `Memory::stack(span<byte>, statistics = Disabled)` | `Stack`; fixed borrowed Buffer, single-threaded |
| `Stack(void*, size_t, statistics = Disabled)` / `Memory::stack(void*, size_t, statistics = Disabled)` | Pointer/size form of the same Buffer strategy |

`Memory` cannot be instantiated directly. `Global` has a private constructor; obtain it only through `Memory::global()`. It remains alive for the process lifetime, with one shared statistics ledger and PMR resource. `Heap` and `Stack` have public constructors and must outlive their Owners, Containers, Allocator/PMR adapters and shared/weak control blocks. These classes cannot be copied or moved. See [lifetime requirements](compatibility.md#lifetime) for Heap/Stack release order.

## 2 · Configuration and queries

| Interface | Result |
| --- | --- |
| `MemoryKind::{Global, Heap, Stack}` / `kind()` | Allocation mode |
| `Memory::config(release_delay_ms)` | Set the backend-wide idle-page release delay |
| `statistics()` / `reset_statistics()` | Five-field value; clear all five counters without freeing storage |

`config(release_delay_ms)` accepts milliseconds; -1 disables automatic page reclamation. Statistics are selected per instance through a factory argument or `set_statistics()`; OFF builds reject Basic.

The required source options are `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` and `UNIMEMORY_STATISTICS=ON|OFF`. Backend selection happens once in CMake; allocation APIs use that choice. ON permits runtime Basic for Global, Heap and Stack; OFF rejects Basic configuration. All modes default to Disabled. Global captures statistics on first access; Heap and Stack capture their own mode at construction. Use `set_statistics()` to change an existing instance mode. `UNIMEMORY_CHECKS=AUTO` enables precondition checks in Debug and omits them in other configurations; ON/OFF override it. Keep one configuration across consumer translation units. [Build options](getting-started.md#build-options)

`Statistics` contains five request counter fields; Disabled returns five zeros. Global lookup does not replace
ordinary `new/delete` or unadapted Containers.

## 3 · Bytes and Block

Alignment defaults to `alignof(std::max_align_t)`. Sizes are `size_t`.

| Signature | Result / contract |
| --- | --- |
| `allocate(bytes, alignment)` | `void*`; zero returns null |
| `allocate_zeroed(bytes, alignment)` | Requested bytes are zero |
| `reallocate(ptr, old_bytes, new_bytes, alignment)` | Preserve prefix; failure preserves storage; zero releases logically |
| `reallocate_zeroed(ptr, old_bytes, new_bytes, alignment)` | Additionally zero newly requested bytes |
| `deallocate(ptr, bytes, alignment)` | `void`, noexcept; original Memory, size/alignment pairing |
| `make_block(bytes, alignment)` | Move-only `OwnedBlock` |

OwnedBlock: `data()`, `size()`, `alignment()`, `resize(new_bytes)`. Growth is
uninitialized. Stack deallocation does not reclaim individual Buffer storage.

## 4 · Object, Array, Owner and Container

| Signature | Result |
| --- | --- |
| `allocate_objects<T>(count = 1)` / `deallocate_objects(ptr, count = 1)` | Unconstructed typed storage; original count |
| `create<T>(args...)` / `destroy(ptr)` | Construct / destroy one Object; exact original type |
| `create_array<T>(count)` / `destroy_array(ptr, count)` | Value-initialized Array; original count |
| `make_unique<T>(args...)` | `Unique<T>` |
| `make_unique_array<T>(count)` | `UniqueArray<T>` |
| `make_shared<T>(args...)` | `std::shared_ptr<T>` |
| `make_shared<T[]>(count)` | `std::shared_ptr<T[]>`, shared array |
| `adopt_unique(ptr)` | `Unique<T>`; existing Object from this Memory, exact original type |
| `adopt_unique_array(ptr, count)` | `UniqueArray<T>`; also original Array count |
| `allocator<T>()` | `Allocator<T>` |
| `resource()` | `std::pmr::memory_resource*` |

Object destructors must not throw. See [ownership and adoption](guides/objects.md) and [container resource selection](guides/containers.md).

## 5 · Heap and Stack controls

| Signature | Supported mode / meaning |
| --- | --- |
| `reset()` | Heap/Stack: bulk release or rewind; counters restart |
| `collect()` | Heap: attempt idle-resource reclamation, preserve live blocks |
| `owns(const void*)` | Heap: this Heap’s live allocation start; null false |
| `mark()` | Stack: opaque `Stack::Mark` |
| `rewind(Mark)` | Stack: recover trailing storage; all old marks become invalid |
| `used()` / `capacity()` | Stack: occupied / total Buffer bytes |

`owns()` is not a general pointer validator. Stack uses the same request-counter contract and has no native metrics. Heap controls exist only on `Heap`; Stack controls exist only on `Stack`. `Memory` and `Global` expose neither set of controls.

## 6 · Errors

| Condition | Behavior |
| --- | --- |
| Allocation/capacity failure | `bad_alloc`; resize preserves old storage |
| Invalid allocation alignment | `invalid_argument` with checks enabled; a caller precondition when checks are OFF |
| Invalid statistics mode, Basic configuration in an OFF build, invalid Buffer or Mark | `invalid_argument` |
| Element-count or mark-counter overflow | `length_error` |
| Native control failure | `runtime_error` |
| Moved-from Block resize | `logic_error` |
| Invalid pointers, mismatched frees, premature reset/destruction | Caller contract violation; not necessarily detected |

Link `UniMemory::UniMemory`. [Build](getting-started.md) · [Lifetime](compatibility.md).
