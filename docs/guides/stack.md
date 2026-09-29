# Stack allocation

[Index](../README.md) · **English** · [简体中文](stack.zh-CN.md)

## Borrow a fixed Buffer

```cpp
#include <unimem/memory.h>

alignas(std::max_align_t) std::byte buffer[4096];
unimem::Memory scratch = unimem::Memory::stack(buffer);
unimem::Memory::Mark checkpoint = scratch.mark();
{
    unimem::OwnedBlock block = scratch.make_block(128, 64);
}
scratch.rewind(checkpoint);
```

Stack means checkpoint-based allocation, not an OS-stack allocation inside the factory.
The Buffer is borrowed, writable, fixed-capacity and single-threaded. It must outlive Memory.
All common Object, Owner, Container and PMR methods are available.

## Reuse storage

| Operation | Behavior |
| --- | --- |
| `allocate()` | Advances the offset, including alignment padding |
| `deallocate()` | Retains Buffer space; does not run Object destructors |
| Owner destruction | Runs applicable Object destructors; retains Buffer space |
| `mark()` | Saves the current offset |
| `rewind(mark)` | Returns to that offset; all old marks become invalid |
| `reset()` | Returns to zero; all old marks become invalid |
| `used()` / `capacity()` | Consumed Buffer bytes / total bytes |

Destroy affected Owners and Containers before rewind/reset. These operations do not
run Object destructors. Foreign or stale marks throw `invalid_argument`.
Marks must not outlive Memory. Do not create active overlapping Buffer allocators.

Rewinding an inner Mark also invalidates outer Marks. This interface uses a single
checkpoint generation, so it does not support nested saved-Marks unwinding.

## Resize and failure

| Case | Result |
| --- | --- |
| Capacity exhausted | `bad_alloc`; offset unchanged; no Heap fallback |
| Block ends at current offset and growth fits | Extends in place |
| Other growth | Allocates/copies within the same Buffer |
| Shrink or free | Retains occupied Buffer space |
| Failed resize | Preserves old pointer, bytes and offset |

Stack has no Backend or native statistics. Use `used()` to inspect occupied bytes;
it includes padding and storage retained after individual releases.

Next: [Containers](containers.md) · [API](../api-reference.md)
