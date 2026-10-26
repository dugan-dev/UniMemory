# Build and integration

[Home](README.md) · [简体中文](getting-started.zh-CN.md)

## Get the source

Use CMake 3.25 or later and a C++20 compiler. Download the [dev source ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip) and extract it, or run:

```sh
git clone --branch dev https://github.com/dugan-dev/UniMemory.git
cd UniMemory
```

Run the following commands from the directory containing `CMakeLists.txt`. The Standard backend needs no allocator SDK or dependency download. Keep headers, generated configuration and optional SDK libraries from matching revisions.

## Build and run

The `release` preset selects `standard`, statistics `OFF` and checks `AUTO`. Enable examples explicitly:

```sh
cmake --preset release -DUNIMEMORY_BUILD_TESTS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
```

The presets require Ninja and build in parallel by default.

On Windows, run in PowerShell:

```powershell
./build/UniMemory-release/UniMemoryExample_basic.exe
./build/UniMemory-release/UniMemoryExample_heap.exe
./build/UniMemory-release/UniMemoryExample_scratch.exe
```

With a single-configuration generator on Linux/macOS:

```sh
./build/UniMemory-release/UniMemoryExample_basic
./build/UniMemory-release/UniMemoryExample_heap
./build/UniMemory-release/UniMemoryExample_scratch
```

Expected output: `3, 1024 bytes`, `Heap reset succeeded`, and `0 bytes used after rewind`. Use the `debug` configure/build presets with the same options for Debug; executables are then placed in `build/UniMemory-debug`.

Without presets, an equivalent Release build is:

```sh
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_TESTS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/local --config Release --parallel
```

## Add the source to a project

Place the checkout in `MyApp/UniMemory`, and create `MyApp/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)
set(UNIMEMORY_BACKEND standard CACHE STRING "UniMemory backend")
set(UNIMEMORY_STATISTICS OFF CACHE BOOL "UniMemory counters")
set(UNIMEMORY_BUILD_TESTS OFF CACHE BOOL "UniMemory tests")
add_subdirectory(UniMemory)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

Save any complete program below as `MyApp/main.cpp`, then run from `MyApp`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run `./build/Release/my_app.exe` with Visual Studio, or `./build/my_app` with a single-configuration generator. `UniMemory::UniMemory` supplies C++20, include paths, generated configuration, threads and the selected SDK link dependency. The library itself is header-only; include `<unimem/all.h>`.

## Install and consume a package

From the UniMemory checkout:

```sh
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_TESTS=OFF
cmake --build build/package --config Release
cmake --install build/package --config Release --prefix build/installed
```

Create a separate application directory containing `main.cpp` and this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)
find_package(UniMemory CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

From that application directory, replace the placeholder with the absolute installation path:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<absolute-UniMemory-path>/build/installed"
cmake --build build --config Release
```

The installation contains headers, generated configuration and CMake package files. No UniMemory static/shared binary is produced, and `BUILD_SHARED_LIBS` does not change its form. Optional mimalloc/jemalloc SDKs must also be discoverable by the consumer; include both installation prefixes in `CMAKE_PREFIX_PATH`, separated by a semicolon. Deploy any dynamically linked SDK alongside the application.

## Examples

These programs work with the Standard profile above. All contexts default to runtime statistics `Disabled`.

### Global: an object, owned bytes and a PMR container

```cpp
#include <unimem/all.h>

#include <iostream>
#include <memory_resource>
#include <vector>

struct Point {
  float x;
  float y;
};

int main() {
  unimem::Global memory;
  Point* point = memory.create<Point>(1.0f, 2.0f);
  const Point position = *point;
  memory.destroy(point);

  unimem::OwnedBlock block = memory.make_block(1024, 64);
  std::pmr::vector<Point> points(memory.resource());
  points.push_back(position);
  std::cout << points.front().x + points.front().y << ", " << block.size() << " bytes\n";
  return 0;
}
```

### Heap: release owners before resetting the group

```cpp
#include <unimem/all.h>

#include <iostream>

int main() {
  unimem::Heap heap;
  {
    unimem::Unique<int> value = heap.make_unique<int>(42);
    if (!heap.owns(value.get()) || *value != 42) {
      return 1;
    }
    heap.collect();
  }
  heap.reset();
  std::cout << "Heap reset succeeded\n";
  return 0;
}
```

`collect()` keeps live allocations valid. `reset()` releases the group's allocations without calling object destructors; destroy objects and release owners/containers first.

### Stack: borrow a buffer and rewind a checkpoint

```cpp
#include <unimem/all.h>

#include <cstddef>
#include <iostream>

int main() {
  alignas(64) std::byte buffer[4096];
  unimem::Stack scratch(buffer, sizeof buffer);
  unimem::Stack::Mark checkpoint = scratch.mark();
  std::byte* bytes = static_cast<std::byte*>(scratch.allocate(128, 64));
  bytes[0] = std::byte{42};
  scratch.rewind(checkpoint);
  std::cout << scratch.used() << " bytes used after rewind\n";
  return scratch.used() == 0 ? 0 : 1;
}
```

The buffer must outlive the Stack and its users. `rewind()` invalidates allocations after the mark and does not call destructors. Destroy affected objects and release their owners before rewinding. Keep each Memory context alive until its objects, owners, allocators and PMR containers are finished.

## Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `UNIMEMORY_BACKEND` | Required | Exactly `standard`, `mimalloc` or `jemalloc` |
| `UNIMEMORY_STATISTICS` | Required | `ON` permits runtime `Basic`; `OFF` rejects it; either way runtime defaults to `Disabled` |
| `UNIMEMORY_CHECKS` | `AUTO` | Checks enabled for Debug only; `ON`/`OFF` override this |
| `UNIMEMORY_AGGRESSIVE_INLINING` | `ON` | Adds `/Ob3` to MSVC Release consumers |
| `UNIMEMORY_BUILD_EXAMPLES` | `OFF` | Builds basic, heap and scratch examples |
| `UNIMEMORY_BUILD_TESTS` | Top-level CTest setting | Use `OFF` for a headers/package-only installation |

Choose one backend/statistics/check-policy profile throughout a program. Use separate build/install directories for different profiles. Installed packages preserve these choices and reject conflicting consumer settings. Do not copy only the source headers or override generated configuration macros.

To gather counters, build with `-DUNIMEMORY_STATISTICS=ON`, then explicitly construct a context with `unimem::StatisticsMode::Basic`, for example `unimem::Heap heap(unimem::StatisticsMode::Basic);`.

Set `-DUNIMEMORY_AGGRESSIVE_INLINING=OFF` to keep your own MSVC Release inlining settings; installed-package consumers can also set this preference.

Sample formatting follows the repository's `.clang-format`: two-space indentation, braces on the same line, left-aligned pointers and a 150-column limit. If clang-format is already installed, format your sample with `clang-format -i main.cpp` from the repository so it uses that configuration.
