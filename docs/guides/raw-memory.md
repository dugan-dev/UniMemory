# Raw memory and alignment

[Index](../README.md) · **English** · [简体中文](raw-memory.zh-CN.md)

Examples: [Quick Start](../../README.md#quick-start).

## Pairing and alignment

| Item | Contract |
| --- | --- |
| Memory | Release through the same Memory used to allocate |
| Size | Supply the current requested byte count after any resize |
| Alignment | Supply the original alignment; nonzero power of two |
| Default alignment | `alignof(std::max_align_t)`, sufficient for fundamental alignment |
| Over-aligned object | Use `alignof(T)`; object helpers do this automatically |
| Request size | Need not be a multiple of alignment |

## Resize and ownership

| Operation | Guarantee |
| --- | --- |
| Resize | Preserve `min(old_bytes, new_bytes)` bytes; the pointer may change |
| Failed resize | Keep the old pointer and contents valid |
| Zeroed growth | Clear only newly requested bytes; preserve existing bytes |
| OwnedBlock | Remember size and alignment; move ownership; release automatically |
| OwnedBlock growth | Newly added bytes are uninitialized |
| Byte ownership | Does not invoke C++ object destructors |

Discard cached `data()` pointers after successful resizing or ownership replacement. Use object helpers for constructed C++ objects.

## Zero and errors

| Input or event | Result |
| --- | --- |
| `allocate(0)` | `nullptr` |
| `reallocate(nullptr, ..., next)` | Allocation |
| Resize to zero | Logical release and `nullptr` |
| `deallocate(nullptr, ...)` | No operation |
| Invalid alignment | `std::invalid_argument` |
| Allocation failure | `std::bad_alloc` |
| Typed count overflow | `std::length_error` |
| Resize a moved-from OwnedBlock | `std::logic_error` |

Stack retains occupied buffer space after individual releases. Container adapters give zero-sized requests a small, pairable allocation. Invalid pointers and mismatched frees violate the contract; detection is not guaranteed.

[Stack behavior](stack.md) · [Ownership](objects.md) · [API](../api-reference.md)
