# API reference

[Index](README.md) · **English** · [简体中文](api-reference.zh-CN.md)

Namespace: `unimem`. Include `<unimem/memory.h>` for the complete header-only allocation API; link the CMake `UniMemory::UniMemory` interface target. `Memory` is a non-template class.

## 1 · Create Memory

| Signature | Result / contract |
| --- | --- |
| `Memory::global(Backend = selected_backend)` | `Memory&`; one library-managed Global; explicit backend must match this build |
| `Memory::heap(Backend, StatisticsMode = Disabled)` | `Memory`; selected backend only; Basic requires statistics ON; Standard unsupported |
| `Memory::stack(span<byte>)` | `Memory`; fixed borrowed Buffer, single-threaded |
| `Memory::stack(void*, size_t)` | Pointer/size form of the same Buffer strategy |

Memory cannot be copied or moved; store global instances by pointer or reference. See [lifetime requirements](compatibility.md#lifetime) for Heap/Stack release order.

## 2 · Configuration and capabilities

| Interface | Result |
| --- | --- |
| `Backend::{Standard, Mimalloc, Jemalloc}` | Backend values; choose one through CMake |
| `Memory::selected_backend` / `selected_statistics` | `static constexpr` configuration values |
| `MemoryKind::{Global, Heap, Stack}` / `kind()` | Allocation mode |
| `backend()` | `optional<Backend>`; empty for Stack |
| `available(backend)` | True only for the selected backend |
| `capabilities(backend)` | `available`, `heap`, `detailed_statistics`, `release_delay` |
| `memory.capabilities()` | `basic_statistics`, `detailed_statistics`, `reset`, `collect`, `owns`, `checkpoints`, `thread_safe`, `individual_reclaim` |
| `supports(backend, RuntimeOption)` | Runtime-option support |
| `set_runtime_option(backend, option, int64_t)` | Unsupported returns false; invalid values/native failure throw |
| `statistics()` / `backend_statistics()` | Optional [statistics](guides/statistics.md) |

The required source options are `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` and `UNIMEMORY_STATISTICS=ON|OFF`. ON means Global Basic; OFF means Global Disabled and rejects Heap Basic. Stack has no counters. `UNIMEMORY_CHECKS=AUTO` enables precondition checks in Debug and omits them in other configurations; ON/OFF override it. Keep one configuration across consumer translation units. [Build options](getting-started.md#build-options)

Current RuntimeOption: `UnusedPageReleaseDelayMs`, milliseconds, Backend-wide.
Capability support is distinct from enabled counters. Global lookup does not replace
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
| `reset()` | Heap/Stack: bulk release or rewind; Heap counters restart |
| `collect()` | Heap: attempt idle-resource reclamation, preserve live blocks |
| `owns(const void*)` | Heap: same-Backend live allocation start; null false |
| `mark()` | Stack: opaque `Memory::Mark` |
| `rewind(Mark)` | Stack: recover trailing storage; all old marks become invalid |
| `used()` / `capacity()` | Stack: occupied / total Buffer bytes |

`owns()` is not a general pointer validator. Stack has no Backend, Basic counters
or native metrics. Unsupported mode-specific operations throw `logic_error`.

## 6 · Errors

| Condition | Behavior |
| --- | --- |
| Allocation/capacity failure | `bad_alloc`; resize preserves old storage |
| Invalid allocation alignment | `invalid_argument` with checks enabled; a caller precondition when checks are OFF |
| Backend mismatch, invalid statistics mode, Heap Basic in an OFF build, invalid Buffer or Mark | `invalid_argument` |
| Element-count or mark-counter overflow | `length_error` |
| Unsupported Heap / native control failure | `runtime_error` |
| Unsupported operation / moved-from Block resize | `logic_error` |
| Invalid pointers, mismatched frees, premature reset/destruction | Caller contract violation; not necessarily detected |

Link `UniMemory::UniMemory`. [Build](getting-started.md) · [Lifetime](compatibility.md).
