# Containers and PMR

[Home](../README.md) · [简体中文](containers.zh-CN.md)

Use C++20, include `<unimem/all.h>` and link `UniMemory::UniMemory`. A `Global`, `Heap` or `Stack` exposes `allocator<T>()` for standard allocator-aware containers and `resource()` for PMR. Libraries without allocator injection retain their own allocator.

## Standard containers

```cpp
#include <unimem/all.h>
#include <vector>

int main() {
    unimem::Global memory;
    std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
    values.push_back(42);
    auto copy = values; // retains this Memory's allocator
    return copy.front() == 42 ? 0 : 1;
} // containers die before memory
```

Adapters compare equal only when bound to the same Memory. Global instances share native allocation facilities but have separate allocator/resource identities. Ordinary container copy retains the UniMemory allocator. Move assignment with unequal allocators may allocate and move elements instead of taking over storage. Swap requires equal allocators/resources whenever the standard container imposes that precondition.

## PMR and nested allocation

```cpp
#include <unimem/all.h>
#include <memory_resource>
#include <string>
#include <vector>

int main() {
    unimem::Global memory;
    std::pmr::vector<std::pmr::string> names(memory.resource());
    names.emplace_back("Ada"); // string receives the vector's resource
    std::pmr::vector<std::pmr::string> copy(names, memory.resource());
    return copy.front() == "Ada" ? 0 : 1;
}
```

`vector<std::string, Allocator<...>>` adapts only vector storage; ordinary strings retain their own allocator. PMR allocator-aware construction can propagate the resource into `pmr::string`. PMR copy construction normally selects the default PMR resource: explicitly pass the destination resource to retain the intended Memory.

## Compose a temporary resource

```cpp
#include <unimem/all.h>
#include <memory_resource>
#include <vector>

int main() {
    unimem::Global memory;
    std::pmr::monotonic_buffer_resource pool(memory.resource());
    std::pmr::vector<int> temporary(&pool);
    temporary.push_back(42);
    std::pmr::vector<int> copy(temporary, &pool);
    return copy.front() == 42 ? 0 : 1;
} // containers, then pool, then memory
```

Allocation follows container → standard PMR resource → Memory resource → backend. The intermediate resource has its own reclamation and threading rules; a thread-safe backend does not make `monotonic_buffer_resource` thread-safe.

## Lifetime and compatibility

Memory instances are non-copyable and non-movable. Keep them alive at stable addresses until containers, adapters, owners and shared/weak control blocks are gone. Destroy containers before intermediate PMR resources, then destroy those resources before Memory. Destroy affected consumers before Heap reset/destruction or Stack rewind/reset, which reclaim storage without running element destructors.

Stack borrows a writable fixed-capacity buffer that must outlive it. Individual deallocations retain buffer space; use one thread and no active overlapping buffer allocators. Global/Heap support concurrent allocation/free, but the same container needs caller synchronization. Heap collection/reset/destruction requires exclusive access.

Across DLL/plugin boundaries preserve the original Memory/resource identity, lifetime and allocation pairing. Keep its defining module and dynamically linked SDK loaded while consumers exist. Passing C++ containers requires compatible compiler, standard-library and runtime ABIs. UniMemory 0.x does not promise stable ABI: use matching headers, generated configuration and libraries, and rebuild consumers after configuration/layout changes.
