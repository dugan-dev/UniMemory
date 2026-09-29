# Getting started

[Index](README.md) · **English** · [简体中文](getting-started.zh-CN.md)

## Build the checkout

Requires a C++20 compiler and CMake 3.25+. No allocator download is needed for Standard.

```sh
git clone https://github.com/dugan-dev/UniMemory.git
cd UniMemory
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

| Platform | Example executable |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS, single configuration | `build/UniMemory-release/UniMemoryExample_basic` |

Run it to print `3, 1024 bytes`. Visual Studio chooses Release through `--config`/the preset; its unused `CMAKE_BUILD_TYPE` warning is harmless.

## Embed the source

Place the checkout inside your project as `UniMemory/`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)
add_subdirectory(UniMemory)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

Use [basic.cpp](../examples/basic.cpp) as `main.cpp`. UniMemory's tests and examples default off in a subproject. No manual include directory or C++ standard flag is needed.

## Install and use a package

```sh
cmake --install build/UniMemory-release --config Release --prefix build/installed
cmake -S tests/consumer -B build/consumer -DCMAKE_PREFIX_PATH="<absolute-path-to-build/installed>"
cmake --build build/consumer --config Release
ctest --test-dir build/consumer -C Release --output-on-failure
```

Replace the placeholder with your installation's absolute path. In an installed consumer:

```cmake
find_package(UniMemory 0.0.1 CONFIG REQUIRED)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

Optional native allocators must also be discoverable when configuring consumers. Shared libraries additionally need their runtime libraries. [Backend setup →](guides/backends.md)

## Configure only what you need

| CMake option | Default | Purpose |
| --- | --- | --- |
| `UNIMEMORY_BUILD_TESTS` | ON for standalone; OFF for subprojects | UniMemory correctness tests |
| `BUILD_TESTING` | ON standalone | OFF also disables UniMemory tests |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | Three small runnable programs |
| `UNIMEMORY_BUILD_BENCHMARKS` | OFF | Allocation benchmarks |
| `BUILD_SHARED_LIBS` | OFF unless supplied by your project | Build a shared library |
| `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC` | OFF | Enable optional backends |

For a library-only build: `cmake -S . -B build/library -DBUILD_TESTING=OFF`.

Next: [Objects](guides/objects.md) · [Containers](guides/containers.md) · [Raw memory](guides/raw-memory.md)
