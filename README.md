# UniMemory

[![CI](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml/badge.svg)](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml) [![Source](https://img.shields.io/badge/source-dev-blue.svg)](https://github.com/dugan-dev/UniMemory/tree/dev) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard) [![CMake](https://img.shields.io/badge/CMake-3.25%2B-green.svg)](https://cmake.org/) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](docs/guides/backends.md)

A modern, header-only C++20 library for unified memory allocation with Standard, mimalloc or jemalloc, selected at build time.

**English** · [简体中文](README.zh-CN.md)

<details>
<summary>Contents</summary>

- [Features](#features)
- [Quick Start](#quick-start)
- [Platforms](#platforms) · [Capabilities](#capabilities)
- [Performance](#performance)
- [Build and Install](#build-and-install) · [Integration](#integration)
- [Documentation](#documentation) · [License](#license)

</details>

## Features

- **🚀 Modern C++20**: Typed construction and standard library integration.
- **🔄 Unified backends**: One non-template Memory API; choose Standard, mimalloc or jemalloc for each build.
- **🧱 Object and Array**: Create and destroy typed objects with constructor failure cleanup.
- **🔒 Smart Pointer**: Create unique and shared pointers; adopt existing objects.
- **📦 Standard Container**: Allocator and PMR adapters for existing C++ containers.
- **🎯 Aligned memory**: Custom alignment, zero initialization and byte-buffer resizing.
- **🗂️ Heap and Stack**: Independent allocation groups and temporary fixed-buffer storage.
- **📊 Optional statistics**: Request counters and available native backend metrics.
- **🌐 Cross-platform**: Supports Windows, Linux and macOS.

## Quick Start

### Basic Usage

```cpp
#include <unimem/memory.h>

int main() {
    // Get global memory for this build
    unimem::Memory& memory = unimem::Memory::global();

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
constexpr unimem::Backend backend = unimem::Memory::selected_backend;

// Create a Heap when supported
if (unimem::available(backend) && unimem::capabilities(backend).heap) {
    unimem::Memory heap = unimem::Memory::heap(backend);

    // Check membership and free
    void* bytes = heap.allocate(1024);
    bool owned = heap.owns(bytes);
    heap.deallocate(bytes, 1024);

    // Pause other operations on this heap while reclaiming memory
    heap.collect();

    // Destroy constructed objects first; reset reclaims storage and invalidates pointers
    void* batch = heap.allocate(1024);
    heap.reset();
}
```

### Stack

```cpp
// Declare the buffer before scratch; use it on this thread only
alignas(std::max_align_t) std::byte buffer[4096];
unimem::Memory scratch = unimem::Memory::stack(buffer);

// Mark and allocate
unimem::Memory::Mark checkpoint = scratch.mark();
void* bytes = scratch.allocate(128);

// Destroy constructed objects first; rewind reclaims storage and invalidates later pointers and old marks
scratch.rewind(checkpoint);

std::size_t used = scratch.used();
std::size_t capacity = scratch.capacity();

// Reset the buffer
scratch.reset();
```

### Initialization and Statistics

```cpp
// Select request counters at build time with UNIMEMORY_STATISTICS=ON/OFF
unimem::Memory& memory = unimem::Memory::global();
unimem::OwnedBlock block = memory.make_block(1024);

// ON: Global Basic counters; OFF: no request statistics
std::optional<unimem::MemoryStatistics> stats = memory.statistics();
if (stats) {
    std::uint64_t live = stats->live_bytes;
    std::uint64_t peak = stats->peak_live_bytes;
    std::uint64_t calls = stats->allocations;
}

// Native fields may be unavailable
std::optional<unimem::BackendStatistics> details = memory.backend_statistics();
if (details && details->committed_bytes) {
    std::uint64_t committed = *details->committed_bytes;
    unimem::BackendStatisticsScope scope = details->scope;
}
```

### Runtime Options

```cpp
constexpr unimem::Backend backend = unimem::Memory::selected_backend;
const unimem::RuntimeOption option = unimem::RuntimeOption::UnusedPageReleaseDelayMs;

// Set backend-wide options before normal allocations
if (unimem::supports(backend, option)) {
    // Delay unused-page release by 1000 ms
    bool configured = unimem::set_runtime_option(backend, option, 1000);
}
```

## Platforms

| Platform | Standard | mimalloc | jemalloc | Compiler |
| --- | :---: | :---: | :---: | --- |
| Windows x64 | ✔ | ✔ | ✔ | MSVC |
| Linux x64 | ✔ | ✔ | ✔ | GCC |
| macOS | ✔ | ✔ | ✔ | Apple Clang |
| Android / iOS | — | — | — | Not device-validated |

✔ Supported; — awaiting device validation. [Platform details](docs/guides/backends.md)

## Capabilities

| Capability | Standard | mimalloc | jemalloc |
| --- | :---: | :---: | :---: |
| Objects, containers, blocks | ✔ | ✔ | ✔ |
| Allocation counters | ✔ | ✔ | ✔ |
| Independent Heap | — | ✔ | ✔ |
| Native statistics scope | — | Process | Process / Heap, build-dependent |
| Unused memory release delay | — | ✔ | ✔ |
| Extra dependency | None | SDK | SDK |

Each build selects one backend. Standard needs no allocator dependency; selecting mimalloc or jemalloc requires its SDK, linked through the interface target. [Configuration guide](docs/guides/backends.md)

## Performance

Windows x64 · MSVC 19.44 · statistics off · 2026-09-28. **ns/operation; lower is faster.**

Historical measurements from the earlier implementation; they do not describe the current header-only build.

| Operation | Standard | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| Create and destroy an object, 64 B | 46.4 | 12.2 | 36.0 |
| Grow and destroy a vector, 32 integers | 590.3 | 237.4 | 488.5 |
| Allocate, resize and free, 4 → 8 KiB | 168.5 | 139.8 | 149.4 |
| Allocate on one thread, free on eight | 105.4 | 59.8 | 166.6 |

![Windows and Linux workload comparison](docs/images/workload-comparison.png)

Standard = 1 in the chart; shorter bars are faster. [Full performance report](docs/performance.md)

## Build and Install

Requires **C++20** and **CMake 3.25+**. Download the [source](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip), then run from the extracted source directory:

```sh
cmake --preset release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_TESTS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
cmake --install build/UniMemory-release --config Release --prefix build/installed
```

The headers and CMake `INTERFACE` target are installed to `build/installed`; UniMemory has no separately compiled library. `UNIMEMORY_BACKEND` and `UNIMEMORY_STATISTICS` must be explicit. `UNIMEMORY_CHECKS=AUTO` enables precondition checks in Debug and omits them in Release. `UNIMEMORY_AGGRESSIVE_INLINING=ON` uses `/Ob3` for MSVC Release consumer translation units; set it to `OFF` to retain your own inlining options. [Build options](docs/getting-started.md#build-options)

## Integration

Link UniMemory in your project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)

find_package(UniMemory 0.0.1 CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

Configure your project with `-DCMAKE_PREFIX_PATH=<absolute-install-path>`; the installed target carries its backend, statistics and check policy. For source integration, set `UNIMEMORY_BACKEND` and `UNIMEMORY_STATISTICS` before `add_subdirectory(UniMemory)`. [Integration guide](docs/getting-started.md)

## Documentation

- **Start:** [Build and install](docs/getting-started.md) · [Runnable examples](examples/README.md)
- **Everyday use:** [Object / Array](docs/guides/objects.md) · [Container](docs/guides/containers.md) · [Block](docs/guides/raw-memory.md)
- **Memory management:** [Heap](docs/guides/heap.md) · [Stack](docs/guides/stack.md) · [Statistics](docs/guides/statistics.md)
- **Reference:** [API](docs/api-reference.md) · [Backend setup](docs/guides/backends.md) · [Full index](docs/README.md)

## License

[MIT](LICENSE). Optional allocators retain their respective licenses.
