# Stack allocation

[Index](../README.md) · **English** · [简体中文](stack.zh-CN.md)

Examples: [Quick Start](../../README.md#quick-start).

## Buffer lifetime

`Memory::stack(buffer)` allocates from an existing buffer and supports all common Memory APIs. The buffer must be writable, fixed-capacity and outlive Memory. Use one thread only. This is not the thread call stack.

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

Rewinding an inner Mark also invalidates outer Marks. Saved Marks cannot be unwound one level at a time.

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
