# Raw memory and alignment

[Index](../README.md) · **English** · [简体中文](raw-memory.zh-CN.md)

## Own a byte block

```cpp
unimem::Memory& memory = unimem::Memory::global();
unimem::OwnedBlock block = memory.make_block(4096, 64);
block.resize(8192);
void* data = block.data();
```

`OwnedBlock` remembers size and alignment, moves ownership and frees automatically. `resize()` preserves the old block on failure. Growth is uninitialized; byte ownership does not run object destructors.

## Pair raw operations

```cpp
void* bytes = memory.allocate_zeroed(64, 32);
bytes = memory.reallocate_zeroed(bytes, 64, 128, 32);
memory.deallocate(bytes, 128, 32);
```

| Operation | Guarantee |
| --- | --- |
| `allocate(bytes, alignment)` | Raw storage; default alignment is `alignof(std::max_align_t)` |
| `allocate_zeroed(...)` | All requested bytes are zero |
| `reallocate(ptr, old, next, alignment)` | Preserves `min(old, next)` bytes; failure preserves the old block |
| `reallocate_zeroed(...)` | Additionally zeroes newly requested bytes |
| `deallocate(ptr, bytes, alignment)` | Use the original Memory, current request size and original alignment |

Alignment is a nonzero power of two. The default covers fundamental alignment; over-aligned types need `alignof(T)`, which object helpers supply automatically. Size need not be a multiple of alignment.

## Zero and failures

| Input or event | Result |
| --- | --- |
| `allocate(0)` | `nullptr` |
| `reallocate(nullptr, ..., next)` | Allocation |
| Resize to zero | Free and return `nullptr` |
| `deallocate(nullptr, ...)` | No operation |
| Invalid alignment | `std::invalid_argument` |
| Allocation failure | `std::bad_alloc` |
| Typed count overflow | `std::length_error` |
| Resize a moved-from OwnedBlock | `std::logic_error` |

Invalid pointer lifetimes and mismatched frees violate the contract; they are not guaranteed to be detected. Container adapters give zero-sized requests a small, pairable allocation. Use object helpers for C++ objects.

Next: [Objects](objects.md) · [API reference](../api-reference.md)
