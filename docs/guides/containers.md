# Containers and PMR

[Index](../README.md) · **English** · [简体中文](containers.zh-CN.md)

## Supply an allocator explicitly

```cpp
#include <unimem/memory.h>
#include <memory_resource>
#include <vector>

unimem::Memory& memory = unimem::Memory::global();
std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
std::pmr::vector<int> numbers(memory.resource());
values.push_back(1);
numbers.push_back(2);
```

| Adapter | Use when | Allocation path |
| --- | --- | --- |
| `Allocator<T>` | The API accepts a standard allocator type | Direct Memory call |
| `resource()` | The API accepts `std::pmr::memory_resource*` | Standard PMR virtual call, then Memory |

The standard library already provides `std::pmr::vector`, `string`, `map` and other aliases. Plain containers and libraries without allocator injection are unaffected.

## Temporary containers

```cpp
unimem::Memory& memory = unimem::Memory::global();
std::pmr::monotonic_buffer_resource pool(memory.resource());
std::pmr::vector<int> temporary(&pool);
```

```mermaid
flowchart LR
    A[Container] --> B[Standard PMR pool]
    B --> C[Memory resource]
    C --> D[Backend]
```

Destroy `temporary` before `pool`; keep the referenced Memory alive. The pool has its own reclamation and threading rules.

## Copy, move and nested types

| Situation | Rule |
| --- | --- |
| Ordinary container copy | The allocator adapter retains its Memory |
| PMR copy construction | The standard container normally chooses the default PMR resource; supply the destination resource explicitly |
| Swap | Allocators/resources must compare equal when the standard container requires it |
| Unequal allocator move assignment | May allocate and move elements; it need not steal storage |
| `vector<std::string, Allocator<...>>` | Only vector storage is adapted; ordinary strings keep their own allocator |
| `pmr::vector<pmr::string>` | Standard allocator-aware construction can propagate the PMR resource to strings |

Keep every referenced Memory alive. Passing C++ containers across dynamic libraries also requires compatible compiler, standard-library and runtime ABIs.

Next: [Scratch storage](stack.md) · [Compatibility](../compatibility.md)
