# UniMemory

[![CI](https://github.com/dugan-dev/UniMemory/actions/workflows/performance.yml/badge.svg?branch=dev&event=push)](https://github.com/dugan-dev/UniMemory/actions/workflows/performance.yml?query=branch%3Adev+event%3Apush) [![Source](https://img.shields.io/badge/source-dev-blue.svg)](https://github.com/dugan-dev/UniMemory/tree/dev) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard) [![CMake](https://img.shields.io/badge/CMake-3.25%2B-green.svg)](https://cmake.org/) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](docs/guides/backends.md)

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
- **🔄 Unified backends**: One API for Standard, mimalloc and jemalloc.
- **🧱 Object and Array**: Create and destroy typed objects with constructor failure cleanup.
- **🔒 Smart Pointer**: Create unique and shared pointers; adopt existing objects.
- **📦 Standard Container**: Allocator and PMR adapters for existing C++ containers.
- **🎯 Aligned memory**: Custom alignment, zero initialization and byte-buffer resizing.
- **🗂️ Heap and Stack**: Independent allocation groups and temporary fixed-buffer storage.
- **📊 Optional statistics**: Optional allocation counts, live bytes and peak usage.
- **🌐 Cross-platform**: Supports Windows, Linux and macOS.

`Memory` provides the common inline allocation interface. `Memory::global()` returns the unique process-lifetime `Global&`; its statistics and PMR resource are shared. Construct `Heap` and `Stack` directly or use `Memory::heap()` and `Memory::stack()`.

## Quick Start

### Basic Usage

```cpp
#include <unimem/all.h>

int main() {
    // Get global memory for this build
    unimem::Global& memory = unimem::Memory::global();

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
unimem::Heap heap;

// Check membership and free
void* bytes = heap.allocate(1024);
bool owned = heap.owns(bytes);
heap.deallocate(bytes, 1024);

// Pause other operations on this heap while reclaiming memory
heap.collect();

// Destroy constructed objects first; reset reclaims storage and invalidates pointers
void* batch = heap.allocate(1024);
heap.reset();
```

### Stack

```cpp
// Declare the buffer before scratch; use it on this thread only
alignas(std::max_align_t) std::byte buffer[4096];
unimem::Stack scratch(buffer);

// Mark and allocate
unimem::Stack::Mark checkpoint = scratch.mark();
void* bytes = scratch.allocate(128);

// Destroy constructed objects first; rewind reclaims storage and invalidates later pointers and old marks
scratch.rewind(checkpoint);

std::size_t used = scratch.used();
std::size_t capacity = scratch.capacity();

// Reset the buffer
scratch.reset();
```

### Initialization and Statistics

Statistics default to Disabled. ON builds permit runtime Basic; select statistics on first Global access or when constructing each Heap/Stack; use `set_statistics()` to change the shared Global mode. Existing instances retain their modes.

```cpp
// Enable request counters with UNIMEMORY_STATISTICS=ON
unimem::Global& memory = unimem::Memory::global();
memory.set_statistics(unimem::StatisticsMode::Basic);
unimem::OwnedBlock block = memory.make_block(1024);
unimem::Statistics stats = memory.statistics();
std::uint64_t live = stats.live_bytes;
std::uint64_t peak = stats.peak_live_bytes;
std::uint64_t calls = stats.allocations;
block.resize(0);
memory.reset_statistics();
```

### Runtime Options

```cpp
unimem::Memory::config(1000);
```

## Platforms

| Platform | Standard | mimalloc | jemalloc | Compiler |
| --- | :---: | :---: | :---: | --- |
| Windows x64 | ✔ | ✔ | ✔ | MSVC |
| Linux x64 | ✔ | ✔ | ✔ | GCC / Clang |
| macOS | ✔ | ✔ | ✔ | Apple Clang |
| Android / iOS | — | — | — | Not device-validated |

✔ Supported; — awaiting validation. [Platform details](docs/guides/backends.md)

## Capabilities

| Capability | Standard | mimalloc | jemalloc |
| --- | :---: | :---: | :---: |
| Objects, containers, blocks | ✔ | ✔ | ✔ |
| Allocation counters | ✔ | ✔ | ✔ |
| Independent Heap | ✔ | ✔ | ✔ |
| Unused memory release delay | — | ✔ | ✔ |
| Extra dependency | None | SDK | SDK |

Standard needs no extra allocator dependency; mimalloc and jemalloc require their SDK. [Configuration guide](docs/guides/backends.md)

## Performance

<!-- performance-overview-source: docs/results/windows-local-comparison.json -->

Windows x64 · MSVC 19.44 · Release with IPO · statistics off · 2026-10-12. Median of 10 trials, **ns/operation; lower is faster**.

| Operation | Standard | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| Create and destroy an object, 64 B | 30.8 | 6.1 | 26.6 |
| Grow and destroy a vector, 256 integers | 233.2 | 242.8 | 181.5 |
| Create and release a block, 64 B | 29.9 | 5.7 | 27.5 |

![Windows and Linux workload comparison](docs/images/workload-comparison.png)

[Local table source](docs/results/windows-local-comparison.json). The chart retains the previously published CI measurements (vector: 32 integers). [Latest Windows comparison](docs/performance-windows.md) · [Full performance report](docs/performance.md)

## Build and Install

Requires **C++20** and **CMake 3.25+**. Download the [source](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip), then run from the extracted source directory:

```sh
cmake --preset release -DUNIMEMORY_BUILD_TESTS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
cmake --install build/UniMemory-release --config Release --prefix build/installed
```

The headers are installed to `build/installed`. The release preset selects Standard with statistics off; see [build options](docs/getting-started.md#build-options) to change them.

## Integration

Link UniMemory in your project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)

find_package(UniMemory 0.0.1 CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

Configure your project with `-DCMAKE_PREFIX_PATH=<absolute-install-path>`. For source integration, set `UNIMEMORY_BACKEND` and `UNIMEMORY_STATISTICS` before `add_subdirectory(UniMemory)`. [Integration guide](docs/getting-started.md)

## Documentation

- **Start:** [Build and install](docs/getting-started.md) · [Runnable examples](examples/README.md)
- **Everyday use:** [Object / Array](docs/guides/objects.md) · [Container](docs/guides/containers.md) · [Block](docs/guides/raw-memory.md)
- **Memory management:** [Heap](docs/guides/heap.md) · [Stack](docs/guides/stack.md) · [Statistics](docs/guides/statistics.md)
- **Reference:** [API](docs/api-reference.md) · [Backend setup](docs/guides/backends.md) · [Full index](docs/README.md)

## License

[MIT](LICENSE). Optional allocators retain their respective licenses.
