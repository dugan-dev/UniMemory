# UniMemory

[![CI](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml/badge.svg)](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml) [![Release](https://img.shields.io/github/v/release/dugan-dev/UniMemory)](https://github.com/dugan-dev/UniMemory/releases/latest) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard) [![CMake](https://img.shields.io/badge/CMake-3.25%2B-green.svg)](https://cmake.org/) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](docs/guides/backends.md)

A modern C++20 library for unified memory allocation across Standard, mimalloc and jemalloc.

**English** · [简体中文](README.zh-CN.md)

<details>
<summary>Contents</summary>

- [Features](#features)
- [Quick Start](#quick-start)
- [Platforms](#platforms) · [Backends](#backends)
- [Performance](#performance) · [Tests](#tests)
- [CMake Integration](#cmake-integration) · [Building](#building)
- [Documentation](#documentation) · [License](#license)

</details>

## Features

- **🚀 Modern C++20**: Typed construction and standard library integration.
- **🔄 Unified backends**: One API for Standard, mimalloc and jemalloc.
- **🧱 Object and Array**: Create and destroy typed objects with constructor failure cleanup.
- **🔒 Smart Pointer**: Create unique and shared pointers; adopt existing objects.
- **📦 Standard Container**: Allocator and PMR adapters for existing C++ containers.
- **🎯 Aligned memory**: Custom alignment, zero initialization and byte-buffer resizing.
- **🗂️ Heap and Stack**: Independent allocation groups and temporary fixed-buffer storage.
- **📊 Optional statistics**: Request counters and available native backend metrics.
- **🌐 Cross-platform**: Windows, Linux and macOS builds verified in CI.

## Quick Start

The Memory context must outlive its owners, containers and weak control blocks; a Stack buffer must outlive its Memory. Destroy affected objects before reset/rewind: reclamation does not run destructors. Global/Heap allocation and free can run concurrently; reclamation requires exclusive access and Stack is single-threaded. [Lifetime and compatibility](docs/compatibility.md)

Run each example separately. The shorter snippets use `memory` from Basic Usage and, where needed, the `Point` definition from Object; put their includes at file scope. In Initialization and Statistics, `configure_global()` must precede the first `global()` call for each backend.

### Basic Usage

```cpp
#include <unimem/memory.h>

int main() {
    // Get global memory, backend choices: Standard, Mimalloc, Jemalloc
    unimem::Memory& memory = unimem::Memory::global(unimem::Backend::Standard);

    // Allocate and free
    void* bytes = memory.allocate(1024);
    memory.deallocate(bytes, 1024);

    // Allocate zeroed memory and free
    void* zeroed = memory.allocate_zeroed(1024);
    memory.deallocate(zeroed, 1024);

    // Allocate aligned memory and free
    void* aligned = memory.allocate(1024, 64);
    memory.deallocate(aligned, 1024, 64);

    // Resize and free with the new size
    void* resized = memory.allocate(1024);
    resized = memory.reallocate(resized, 1024, 2048);
    memory.deallocate(resized, 2048);

    // Grow and zero the added bytes
    void* grown = memory.allocate_zeroed(2048);
    grown = memory.reallocate_zeroed(grown, 2048, 4096);
    memory.deallocate(grown, 4096);

    return 0;
}
```

### Object

```cpp
struct Point {
    float x;
    float y;
};

// Create and destroy an object
Point* point = memory.create<Point>(1.0f, 2.0f);
memory.destroy(point);

// Allocate typed storage; construct elements before accessing them
Point* storage = memory.allocate_objects<Point>();
memory.deallocate_objects(storage);

// Create and destroy an array
Point* array = memory.create_array<Point>(8);
memory.destroy_array(array, 8);
```

Typed storage establishes the array storage without constructing its elements. [C++20 lifetime contract](docs/typed-storage-lifetime.md)

### Block

```cpp
// Create an owned block aligned to 64 bytes
unimem::OwnedBlock block = memory.make_block(1024, 64);

// Access data, size and alignment
void* data = block.data();
std::size_t bytes = block.size();
std::size_t alignment = block.alignment();

// Resize, the data pointer may change
block.resize(2048);

// Transfer ownership
unimem::OwnedBlock moved = std::move(block);
```

### Container

```cpp
#include <list>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <vector>

// Use a standard Allocator
std::vector<Point, unimem::Allocator<Point>> positions(memory.allocator<Point>());
positions.emplace_back(1.0f, 2.0f);

std::list<Point, unimem::Allocator<Point>> path(memory.allocator<Point>());

// Use standard PMR containers
std::pmr::vector<Point> points(memory.resource());
points.emplace_back(3.0f, 4.0f);
points.resize(8);

std::pmr::string text("Hello", memory.resource());
std::pmr::unordered_map<int, Point> lookup(memory.resource());
```

### Smart Pointer

```cpp
// Create a unique pointer
unimem::Unique<Point> owner = memory.make_unique<Point>(1.0f, 2.0f);

// Create a shared pointer
std::shared_ptr<Point> shared = memory.make_shared<Point>(3.0f, 4.0f);

// Adopt an object from the same Memory
Point* point = memory.create<Point>(5.0f, 6.0f);
unimem::Unique<Point> adopted = memory.adopt_unique(point);

// Use standard weak pointers
std::weak_ptr<Point> weak = shared;

// Create an owned array
unimem::UniqueArray<Point> owned = memory.make_unique_array<Point>(16);
```

### Heap

```cpp
const unimem::Backend backend = unimem::Backend::Mimalloc;

// Create a Heap when supported
if (unimem::capabilities(backend).heap) {
    unimem::Memory heap = unimem::Memory::heap(backend);

    // Check membership and free
    void* bytes = heap.allocate(1024);
    bool owned = heap.owns(bytes);
    heap.deallocate(bytes, 1024);

    // Reclaim unused memory
    heap.collect();

    // Release all blocks, old pointers expire
    void* batch = heap.allocate(1024);
    heap.reset();
}
```

### Stack

```cpp
// Buffer must outlive Memory, one thread only
alignas(std::max_align_t) std::byte buffer[4096];
unimem::Memory scratch = unimem::Memory::stack(buffer);

// Mark and allocate
unimem::Memory::Mark checkpoint = scratch.mark();
void* bytes = scratch.allocate(128);

// Rewind: allocations after the checkpoint and all old marks expire
scratch.rewind(checkpoint);

std::size_t used = scratch.used();
std::size_t capacity = scratch.capacity();

// Reset the buffer
scratch.reset();
```

### Initialization and Statistics

```cpp
#include <vector>

const unimem::Backend backends[] = {
    unimem::Backend::Standard,
    unimem::Backend::Mimalloc,
    unimem::Backend::Jemalloc
};

// Keep the available Memory instances
std::vector<unimem::Memory*> memories;

for (unimem::Backend backend : backends) {
    // Skip backends not included in the build
    if (!unimem::available(backend)) {
        continue;
    }

    // Enable counters before the first global() call
    unimem::Memory::configure_global(backend, unimem::StatisticsMode::Basic);
    memories.push_back(&unimem::Memory::global(backend));
}

// Use each saved Memory and query its statistics
for (unimem::Memory* memory : memories) {
    unimem::OwnedBlock block = memory->make_block(1024);

    // Query request counts and bytes
    std::optional<unimem::MemoryStatistics> stats = memory->statistics();
    if (stats) {
        std::uint64_t live = stats->live_bytes;
        std::uint64_t peak = stats->peak_live_bytes;
        std::uint64_t calls = stats->allocations;
    }

    // Native fields may be unavailable
    std::optional<unimem::BackendStatistics> details = memory->backend_statistics();
    if (details && details->committed_bytes) {
        std::uint64_t committed = *details->committed_bytes;
        unimem::BackendStatisticsScope scope = details->scope;
    }
}
```

### Runtime Options

```cpp
const unimem::Backend backend = unimem::Backend::Mimalloc;
const unimem::RuntimeOption option = unimem::RuntimeOption::UnusedPageReleaseDelayMs;

// Set backend-wide options before normal allocations
if (unimem::supports(backend, option)) {
    // Delay unused-page release by 1000 ms
    bool configured = unimem::set_runtime_option(backend, option, 1000);
}
```

## Platforms

| Platform | Compiler | Available backends |
| --- | --- | --- |
| Windows x64 | MSVC | Standard, mimalloc, jemalloc |
| Linux x64 | GCC | Standard, mimalloc, jemalloc |
| macOS | Apple Clang | Standard, mimalloc, jemalloc |
| Android / iOS | — | Not device-validated |

[Platform and build requirements](docs/guides/backends.md)

## Backends

| Capability | Standard | mimalloc | jemalloc |
| --- | :---: | :---: | :---: |
| Objects, containers, blocks | ✓ | ✓ | ✓ |
| Allocation counters | ✓ | ✓ | ✓ |
| Independent Heap | — | ✓ | ✓ |
| Native statistics scope | — | Process | Process / Heap, build-dependent |
| Unused memory release delay | — | ✓ | ✓ |
| Extra dependency | None | Optional | Optional |

Enable optional backends at build time; use `available()` and `capabilities()` to check compiled support and capabilities. Ordinary `new` and unadapted containers keep their existing allocator. Private-prefix shared backends require an appropriate runtime library search path. [Configuration guide](docs/guides/backends.md) · [jemalloc initialization](docs/backends/jemalloc.md#initialization)

## Performance

Historical measurements from **2026-09-28**: Windows x64, MSVC 19.44, Xeon w9-3595X; statistics off. **Nanoseconds per operation; lower is faster.** These values were not remeasured for the October repairs.

| Operation | Standard | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| Create and destroy an object, 64 B | 46.4 | 12.2 | 36.0 |
| Grow and destroy a vector, 32 integers | 590.3 | 237.4 | 488.5 |
| Allocate, resize and free, 4 → 8 KiB | 168.5 | 139.8 | 149.4 |
| Allocate on one thread, free on eight | 105.4 | 59.8 | 166.6 |

![Windows and Linux workload comparison](docs/images/workload-comparison.png)

In the chart, Standard = 1; shorter bars are faster. Results apply to the measured workloads. [Full report](docs/performance.md)

## Tests

Repair verification from **2026-10-08 to 2026-10-09**; counts below identify the configurations actually tested.

| Validation | Result |
| --- | --- |
| Windows: three backends + examples | **1666/1666** in Release and Debug |
| Linux: three backends + examples | **1667/1667**, installed consumers **7/7** |
| macOS: three backends + examples | [GitHub CI passed](https://github.com/dugan-dev/UniMemory/actions/runs/37871859097) |
| ASan / UBSan, Standard and Stack | **715/715**, leak detection enabled |
| ThreadSanitizer, Standard and Stack | [Release validation passed](https://github.com/dugan-dev/UniMemory/actions/runs/37871859066) |

Shared builds passed **1664/1664** on Windows and **1666/1666** on Linux. All six local configurations passed **7/7** installed consumers. Coverage includes everyday use, typed storage, independent public headers, package discovery, boundaries, exceptions and concurrency. Each CI link verifies its recorded revision. [Full test report](docs/testing.md)

---

## CMake Integration

### Using source

```cmake
add_subdirectory(UniMemory)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

The target provides include paths and C++20 settings. [Installation guide](docs/getting-started.md)

### Using an installed package

```cmake
find_package(UniMemory 0.0.1 CONFIG REQUIRED)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

Optional lookup with `find_package(UniMemory QUIET CONFIG)` reports `UniMemory_FOUND=FALSE` when a compiled backend dependency is unavailable. Match compiler, standard library and runtime configurations. Version 0.x does not promise a stable ABI: distribute matching headers/libraries and rebuild consumers after layout changes. [Compatibility](docs/compatibility.md)

## Building

Requires **C++20**, **CMake 3.25+** and a C++ compiler.

The [2026-10-09 repair source snapshot](https://github.com/dugan-dev/UniMemory/releases/tag/snapshot-2026-10-09) contains the verified repairs and retains library version **0.0.1**. The earlier [v0.0.1 release](https://github.com/dugan-dev/UniMemory/releases/tag/v0.0.1) remains a historical snapshot. Record the source revision as well as the version; an extracted source archive can run the CMake commands below without cloning.

```sh
git clone https://github.com/dugan-dev/UniMemory.git
cd UniMemory
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

Standard needs no external allocator. Optional backends require their existing dependencies. The `release` preset uses `build/UniMemory-release` and selects Release for multi-configuration generators. Run `python tools/check-docs.py` before publishing.

---

## Documentation

- **Start:** [Build and install](docs/getting-started.md) · [Runnable examples](examples/README.md)
- **Everyday use:** [Object / Array](docs/guides/objects.md) · [Container](docs/guides/containers.md) · [Block](docs/guides/raw-memory.md)
- **Memory management:** [Heap](docs/guides/heap.md) · [Stack](docs/guides/stack.md) · [Statistics](docs/guides/statistics.md)
- **Reference:** [API](docs/api-reference.md) · [Backend setup](docs/guides/backends.md) · [Full index](docs/README.md)
- **Maintenance:** [Contributing](CONTRIBUTING.md) · [Security policy](SECURITY.md) · [Private vulnerability reports](https://github.com/dugan-dev/UniMemory/security/advisories/new)

## License

[MIT](LICENSE). Optional allocators retain their respective licenses.
