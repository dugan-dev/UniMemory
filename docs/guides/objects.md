# Allocation and ownership

[Home](../README.md) · [简体中文](objects.zh-CN.md)

Use C++20, include `<unimem/all.h>` and link `UniMemory::UniMemory`. Construct a `Global`, `Heap` or `Stack` directly. Every object, owner, container, adapter and shared/weak control block must finish using its Memory before destruction or storage reset.

## Bytes and owned blocks

| API | Behavior |
| --- | --- |
| `allocate(bytes, alignment)` | Uninitialized bytes |
| `allocate_zeroed(bytes, alignment)` | Zeroed requested bytes |
| `reallocate(ptr, old_bytes, new_bytes, alignment)` | Preserve `min(old_bytes, new_bytes)` bytes; pointer may change |
| `reallocate_zeroed(...)` | Also clear newly requested bytes |
| `deallocate(ptr, bytes, alignment)` | Release through the original Memory |
| `make_block(bytes, alignment)` | Move-only `OwnedBlock`, released automatically |

Alignment defaults to `alignof(std::max_align_t)` and must be a nonzero power of two. Use `alignof(T)` for over-aligned types; byte counts need not be alignment multiples. Release with the current requested size and original alignment. Never mix with `delete`, `free` or another Memory.

`OwnedBlock` exposes `data()`, `size()`, `alignment()` and `resize(new_bytes)`. Growth is uninitialized. Discard cached data pointers after successful resize or ownership replacement. Byte release never runs object destructors; raw resizing does not move-construct objects.

```cpp
#include <unimem/all.h>
#include <cassert>
#include <cstring>

int main() {
    unimem::Global memory;
    auto block = memory.make_block(16);
    std::memset(block.data(), 7, block.size());
    block.resize(32);
    assert(static_cast<unsigned char*>(block.data())[0] == 7);
    void* raw = memory.allocate_zeroed(16, 64);
    try {
        raw = memory.reallocate_zeroed(raw, 16, 32, 64);
    } catch (...) {
        memory.deallocate(raw, 16, 64);
        throw;
    }
    memory.deallocate(raw, 32, 64);
} // block dies before memory
```

## Objects and typed storage

| API | Contract |
| --- | --- |
| `create<T>(args...)` / `destroy(ptr)` | Forward constructor arguments; destroy the exact original concrete type |
| `create_array<T>(count)` / `destroy_array(ptr, count)` | Value-initialize with `T()`; use original count |
| `allocate_objects<T>(count = 1)` / `deallocate_objects(ptr, count = 1)` | Typed array storage without element construction or initialization |

Object helpers require non-throwing destructors. Keep the allocation-start pointer: a `Derived` object must not be passed to `destroy(Base*)`, even with a virtual destructor. Failed construction releases storage; failed array construction destroys completed elements in reverse order. On Stack, releases retain consumed buffer space.

Typed allocation and `Allocator<T>::allocate()` establish array storage under C++20, including after Stack reuse. Construct nontrivial elements before access, then destroy them before release.

```cpp
#include <unimem/all.h>
#include <memory>
#include <string>

int main() {
    unimem::Global memory;
    int* values = memory.create_array<int>(3); // three zeroes
    memory.destroy_array(values, 3);
    auto* text = memory.allocate_objects<std::string>();
    try {
        std::construct_at(text, "hello");
    } catch (...) {
        memory.deallocate_objects(text);
        throw;
    }
    std::destroy_at(text);
    memory.deallocate_objects(text);
}
```

## Smart ownership and adoption

| API | Result |
| --- | --- |
| `make_unique<T>(args...)` | `Unique<T>` |
| `make_unique_array<T>(count)` | `UniqueArray<T>`, value-initialized elements |
| `make_shared<T>(args...)` / `make_shared<T[]>(count)` | Standard shared pointer / shared array |
| `adopt_unique(ptr)` / `adopt_unique_array(ptr, count)` | Transfer cleanup responsibility without constructing or allocating |

Adopt only objects from the same Memory, or released from a matching owner, with the original concrete type and array count. Adoption does not inspect arbitrary pointer provenance. An empty default Unique is valid; assign a bound owner before giving it an object. A nonempty unbound deleter violates the contract and terminates with checks ON. Array `reset(new_pointer)` retains the old count; assign a newly adopted array when the count changes.

```cpp
#include <unimem/all.h>
#include <memory>

struct Base { virtual ~Base() = default; };
struct Derived : Base { int value = 42; };

int main() {
    unimem::Global memory;
    auto unique = memory.make_unique<Derived>();
    Base* view = unique.get(); // keep Unique<Derived> as owner
    (void)view;
    std::shared_ptr<Base> shared = memory.make_shared<Derived>();
    auto adopted = memory.adopt_unique(memory.create<int>(42));
    auto array = memory.make_unique_array<int>(3);
} // owners and control blocks die before memory
```

Shared conversion to Base retains concrete-type cleanup. Remaining weak pointers retain control blocks and must also disappear before Memory destruction or reset. `make_shared` delegates to `std::allocate_shared`; rollback follows the standard library. Tested libstdc++ 13.3 destroys partially constructed shared-array elements in forward order. Use `create_array` or `make_unique_array` when reverse-order rollback is required.

## Errors and threads

Zero-byte allocation returns null; null deallocation does nothing. Reallocating null allocates; resizing to zero releases logically and returns null. Failed resize preserves the old pointer and contents. Allocation/capacity failure throws `std::bad_alloc`, typed-count overflow `std::length_error`, and moved-from block resize `std::logic_error`. Invalid alignment throws `std::invalid_argument` with checks ON and is a caller precondition with checks OFF. Invalid pointers and mismatched frees are not necessarily detected.

Global and Heap support concurrent allocation/free; Stack is single-threaded. Synchronize access to the same object/block separately. Separate shared/weak handles use standard control-block synchronization; modifying the same handle requires synchronization or standard atomic shared-pointer facilities. Publish ownership with a mutex, barrier or release/acquire synchronization. Destroy affected objects, owners, containers and weak references before Heap reset or Stack rewind/reset, which do not run destructors.
