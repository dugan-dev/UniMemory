# Objects and ownership

[Index](../README.md) · **English** · [简体中文](objects.zh-CN.md)

## Prefer an owner

```cpp
struct Point {
    float x;
    float y;
};

unimem::Memory& memory = unimem::Memory::global();
unimem::Unique<Point> point = memory.make_unique<Point>(1.0f, 2.0f);
unimem::UniqueArray<Point> points = memory.make_unique_array<Point>(8);
std::shared_ptr<Point> shared = memory.make_shared<Point>(1.0f, 2.0f);
```

Include `<unimem/memory.h>`. Declare `memory` before its owners.

| Need | Interface | Cleanup |
| --- | --- | --- |
| One object | `make_unique<T>(args...)` | `Unique<T>` automatically destroys and frees |
| Array | `make_unique_array<T>(count)` | `UniqueArray<T>` remembers the element count |
| Shared ownership | `make_shared<T>(args...)` | Standard `shared_ptr`, allocated with this Memory |
| Raw object pointer | `create<T>(args...)` / `destroy(ptr)` | Same Memory and exact original type |
| Take over an existing Object / Array | `adopt_unique(ptr)` / `adopt_unique_array(ptr, count)` | Owner retains Memory and original Array count |
| Unconstructed storage | `allocate_objects<T>(count = 1)` | Caller constructs/destroys, then `deallocate_objects` |

Arrays use `T()` initialization; the example's Point members start at zero. Constructors receive your arguments directly. If construction throws, storage and successfully constructed array elements are cleaned up. Object helpers require non-throwing destructors.

## Manual ownership

```cpp
Point* point = memory.create<Point>(1.0f, 2.0f);
memory.destroy(point);

Point* points = memory.create_array<Point>(8);
memory.destroy_array(points, 8);
```

Use the same Memory, exact original type and original array count. Keep the allocation-start pointer: creating Derived and later destroying through Base* is not supported, even with a virtual destructor. Do not pass these pointers to ordinary `delete`. `reallocate` moves raw bytes; it does not move-construct objects.

## Adopt existing ownership

```cpp
Point* raw = memory.create<Point>(1.0f, 2.0f);
unimem::Unique<Point> owner = memory.adopt_unique(raw);
unimem::UniqueArray<Point> array = memory.adopt_unique_array(memory.create_array<Point>(8), 8);
```

Adoption does not allocate or construct Objects. Only adopt an Object created by
this Memory, or one released from a matching owner. Ordinary `new` storage is not
compatible. Default Unique owners are empty; assigning an adopted owner binds its
deleter. A nonempty unbound deleter terminates deterministically. For Array owners,
`reset(new_pointer)` retains the old element count; assign a newly adopted Array
when its count changes.

For polymorphic shared ownership, `std::shared_ptr<Base> owner = memory.make_shared<Derived>();`
retains the concrete allocation. Keep `Unique<Derived>` as the owner when only a
non-owning Base view is needed.

On Stack, failed construction destroys built Objects but retains Buffer consumption;
destroy other affected owners before a deliberate rewind/reset.

Both `shared_ptr` **and remaining `weak_ptr` control blocks** must be gone before their Heap/Stack Memory is destroyed/reset.

Next: [Containers](containers.md) · [Raw memory](raw-memory.md)
