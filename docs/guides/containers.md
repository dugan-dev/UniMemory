# Containers and PMR

[Index](../README.md) · **English** · [简体中文](containers.zh-CN.md)

Examples: [Quick Start](../../README.md#container).

## Adapter choice

| Method | Use with |
| --- | --- |
| `allocator<T>()` | Standard allocator-aware containers |
| `resource()` | PMR containers and resources |

Containers and third-party libraries without allocator injection retain their own allocator. Adapters compare equal only when bound to the same Memory.

## Copy, move and nested types

| Situation | Rule |
| --- | --- |
| Ordinary container copy | The allocator adapter retains its Memory |
| PMR copy construction | Normally selects the default PMR resource; explicitly supply the destination resource to retain the selected Memory |
| Swap | Allocators/resources must compare equal when the standard container requires it |
| Unequal allocator move assignment | May allocate and move elements rather than take over storage |
| `vector<std::string, Allocator<...>>` | Only vector storage is adapted; ordinary strings keep their own allocator |
| `pmr::vector<pmr::string>` | Standard allocator-aware construction can propagate the resource into strings |

## Compose a temporary resource

```cpp
#include <unimem/memory.h>

int main() {
    unimem::Memory& memory = unimem::Memory::global(unimem::Backend::Standard);

    // Place a standard PMR resource above UniMemory
    std::pmr::monotonic_buffer_resource pool(memory.resource());
    std::pmr::vector<int> temporary(&pool);
    temporary.push_back(42);

    // Choose the destination resource explicitly when copying
    std::pmr::vector<int> copy(temporary, &pool);
    return copy.front() == 42 ? 0 : 1;
}
```

```mermaid
flowchart LR
    A[Container] --> B[Standard PMR resource]
    B --> C[Memory resource]
    C --> D[Backend]
```

Destroy containers before the PMR resource, and the resource before Heap/Stack Memory. The resource has its own reclamation and threading rules. Passing C++ containers across dynamic libraries also requires compatible compiler, standard-library and runtime ABIs.

[Lifetime and threads](../compatibility.md) · [Stack rules](stack.md) · [API](../api-reference.md)
