# API reference

[Index](README.md) · **English** · [简体中文](api-reference.zh-CN.md)

Namespace: `unimem`. Include `<unimem/memory.h>` for the allocation API.

## 1 · Create Memory

| Signature | Result / contract |
| --- | --- |
| `Memory::global(Backend = Standard)` | `Memory&`; shared instance, library-managed |
| `Memory::configure_global(Backend, StatisticsMode)` | Configure before first lookup; mode changes afterward throw |
| `Memory::heap(Backend, StatisticsMode = Disabled)` | `Memory`; independent native Heap; Standard unsupported |
| `Memory::stack(span<byte>)` | `Memory`; fixed borrowed Buffer, single-threaded |
| `Memory::stack(void*, size_t)` | Pointer/size form of the same Buffer strategy |

Constructors are private. Memory is non-copyable, non-movable and non-polymorphic.
Factories return directly constructed values. Destroy affected Owners before Heap
destruction/reset or Stack rewind/reset; bulk release does not invoke destructors.

## 2 · Configuration and capabilities

| Interface | Result |
| --- | --- |
| `Backend::{Standard, Mimalloc, Jemalloc}` | Backend choice |
| `MemoryKind::{Global, Heap, Stack}` / `kind()` | Allocation mode |
| `backend()` | `optional<Backend>`; empty for Stack |
| `available(backend)` | Backend enabled in this build |
| `capabilities(backend)` | `available`, `heap`, `detailed_statistics`, `release_delay` |
| `memory.capabilities()` | `basic_statistics`, `detailed_statistics`, `reset`, `collect`, `owns`, `checkpoints`, `thread_safe`, `individual_reclaim` |
| `supports(backend, RuntimeOption)` | Runtime-option support |
| `set_runtime_option(backend, option, int64_t)` | Unsupported returns false; invalid values/native failure throw |
| `statistics()` / `backend_statistics()` | Optional [statistics](guides/statistics.md) |

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
| `make_shared<T>(args...)` | `std::shared_ptr<T>` using `std::allocate_shared` |
| `adopt_unique(ptr)` | `Unique<T>`; existing Object from this Memory, exact original type |
| `adopt_unique_array(ptr, count)` | `UniqueArray<T>`; also original Array count |
| `allocator<T>()` | `Allocator<T>` |
| `resource()` | `std::pmr::memory_resource*` |

Object helpers require nothrow destructors and roll back partial construction.
Stack retains Buffer consumption after construction failure. Default Unique owners
are empty; nonempty unbound deleters or nonempty zero-count Array deleters terminate.
Allocator/Resource equality uses Memory identity. Adapters and remaining weak-pointer
control blocks must not outlive their Memory.

Basic snapshots support concurrent monitoring. Backend diagnostic snapshots
require paused activity and thread cleanup in their Memory/Process scope.

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
| Invalid alignment, enum, Buffer or Mark | `invalid_argument` where validated |
| Typed size or Mark-generation overflow | `length_error` |
| Unavailable Backend / unsupported Heap / native control failure | `runtime_error` |
| Unsupported operation / late Global mode change / moved-from Block resize | `logic_error` |
| Invalid pointers, mismatched frees, premature reset/destruction | Caller contract violation; not necessarily detected |

Link `UniMemory::UniMemory`. [Build](getting-started.md) · [Lifetime](compatibility.md).
