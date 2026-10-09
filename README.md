# UniMemory

[![CI](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml/badge.svg)](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml) [![Release](https://img.shields.io/github/v/release/dugan-dev/UniMemory)](https://github.com/dugan-dev/UniMemory/releases/latest) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard) [![CMake](https://img.shields.io/badge/CMake-3.25%2B-green.svg)](https://cmake.org/) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](docs/guides/backends.md)

A C++20 memory library for objects, containers and byte buffers, with a common interface over Standard, mimalloc and jemalloc.

**English** · [简体中文](README.zh-CN.md)

## Features

- Typed object/array construction, exception cleanup and unique/shared ownership.
- Allocator and PMR adapters for standard C++ containers.
- Global allocation, independent Heap contexts and fixed-buffer Stack allocation.
- Alignment, owned buffers, resizing and optional allocation statistics.
- Mature explicit backends; no global `new`/`delete` replacement.

## Quick Start

```cpp
#include <unimem/memory.h>
#include <vector>

struct Point { float x, y; };

int main() {
    auto& memory = unimem::Memory::global();
    auto point = memory.make_unique<Point>(1.0f, 2.0f);
    std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
    values.push_back(20);
    auto bytes = memory.make_block(1024, 64);
    return point->x == 1.0f && values[0] == 20 && bytes.size() == 1024 ? 0 : 1;
}
```

The Memory context must outlive its owners, containers and weak control blocks. Stack buffers must outlive their Memory. Destroy affected objects before reset/rewind: reclamation does not run their destructors. Global/Heap allocation and free can be concurrent; reclamation is exclusive and Stack is single-threaded. See [lifetime and compatibility](docs/compatibility.md).

## CMake Integration

From source:

```cmake
add_subdirectory(UniMemory)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

Installed package:

```cmake
find_package(UniMemory 0.0.1 CONFIG REQUIRED)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

The target supplies headers and C++20 settings. Optional lookup with `find_package(UniMemory QUIET CONFIG)` reports `UniMemory_FOUND=FALSE` when a compiled backend dependency is unavailable. Keep compiler/runtime configurations consistent. Version 0.x requires matching headers/libraries and rebuilt consumers; published tags are immutable snapshots, while main may contain later fixes.

## Backends and Platforms

| Capability | Standard | mimalloc | jemalloc |
|---|:---:|:---:|:---:|
| Objects, containers and blocks | Yes | Yes | Yes |
| Request counters | Yes | Yes | Yes |
| Independent Heap | No | Yes | Yes |
| Extra dependency | None | Optional | Optional |

Standard is always available. Enable optional backends with `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC`; check `available()` and `capabilities()` at runtime. Windows, Linux and macOS are configured in CI; use the linked test reports for the revision actually verified. Private-prefix shared backends need an appropriate runtime library search path. [Backend setup](docs/guides/backends.md)

## Build and Test

Requires CMake 3.25+, C++20 and a compatible compiler.

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/release --parallel 4
ctest --test-dir build/release --output-on-failure
cmake --install build/release --prefix build/installed
```

With Visual Studio, add `--config Release` / `-C Release`. Run `python tools/check-docs.py` before publishing. Tests cover public headers, typed storage reuse, optional package discovery, object lifetime, boundaries, exceptions, concurrency and installed consumers. [Test report](docs/testing.md) · [C++20 typed-storage proof](docs/typed-storage-lifetime.md)

## Performance

The common interface, ownership and enabled counters have measurable costs. Performance and memory results depend on workload, backend and platform; no universal fastest/lowest-memory claim is made. Existing measurements and reproducible fixtures are retained in the [performance report](docs/performance.md).

## Documentation

- [Build and install](docs/getting-started.md) · [Runnable examples](examples/README.md)
- [Objects and arrays](docs/guides/objects.md) · [Containers](docs/guides/containers.md) · [Byte buffers](docs/guides/raw-memory.md)
- [Heap](docs/guides/heap.md) · [Stack](docs/guides/stack.md) · [Statistics](docs/guides/statistics.md)
- [API reference](docs/api-reference.md) · [Full documentation](docs/README.md)
- [Contributing](CONTRIBUTING.md) · [Security reports](SECURITY.md)

## License

[MIT](LICENSE). Optional allocators retain their respective licenses.
