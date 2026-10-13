# Platforms and backends

[Home](../README.md) · [简体中文](backends.zh-CN.md)

## Platform support

UniMemory requires C++20 and CMake 3.25 or later. The platform coverage recorded by the project is:

| Platform/compiler | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| Windows x64 / MSVC | Validated | Validated | Validated |
| Windows ARM64 / MSVC, clang-cl | Validated | Not validated | Not validated |
| Linux x64 / GCC | Validated | Validated | Validated with `je_` prefix |
| Linux ARM64 / GCC, Clang | Validated | Not validated | Not validated |
| macOS Apple Silicon / AppleClang | Validated | Validated | Validated with `je_` prefix |
| macOS Intel / AppleClang | Validated | Not validated | Not validated |
| Android / iOS | Not device-tested | Not device-tested | Not device-tested |

Coverage does not guarantee every OS version or architecture. Cross-compiling also requires a compiler/toolchain, allocator SDK and deployment binaries built for the target platform. Do not use host allocator libraries in a target build.

## Select a build profile

Always set both `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` and `UNIMEMORY_STATISTICS=ON|OFF`. There is no implicit CMake backend or statistics setting. The repository's `release` and `debug` presets explicitly select `standard` and `OFF`.

| Backend | SDK version used by the project | Requirements |
| --- | --- | --- |
| `standard` | C++20 standard library | Process-global `new/delete`; no allocator SDK |
| `mimalloc` | 3.4.3 | v3.4.3+ headers and matching library; newer versions need their own validation |
| `jemalloc` | 5.3.1 | Matching headers/library exporting `je_` functions, without replacing global C++ `new/delete` |

The vcpkg manifest retains baseline `2f1d605400c8727cc00c15797aba796c88ccd523`; Windows CI uses vcpkg checkout `2273a28f34ce5aac8be50b3b6b44da7fc1722e06`. Reusing other installed packages does not apply these pins retroactively.

`Global`, `Heap` and `Stack` are directly constructed contexts sharing the `Memory` interface. Global instances share the native allocator while keeping separate statistics and PMR resource identities. Heap provides an independent allocation group on every backend. Stack allocates from a caller-owned fixed buffer. The backend is chosen for the whole program at build time, not through a public runtime selector.

## Standard: no SDK setup

From a UniMemory checkout:

```sh
cmake -S . -B build/standard -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/standard --config Release --parallel
ctest --test-dir build/standard -C Release --output-on-failure
```

These commands use local tools only. For a package-only build, add `-DUNIMEMORY_BUILD_TESTS=OFF`.

## Windows SDKs with vcpkg

Use your existing vcpkg checkout and set `VCPKG_ROOT` to its absolute path. From the UniMemory checkout, run in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:/tools/vcpkg'
cmake -S . -B build/mimalloc "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/mimalloc --config Release --parallel
```

Manifest installation may download dependencies. Fresh mimalloc Windows x64 builds default to `x64-windows-unimemory`, which disables CRT redirection. An explicitly supplied triplet remains unchanged; ensure it also builds mimalloc without global allocation replacement. To select jemalloc, use `-DUNIMEMORY_BACKEND=jemalloc` and a separate directory such as `build/jemalloc`.

To reuse already installed packages without changing that package set:

```powershell
cmake -S . -B build/mimalloc-existing `
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_INSTALLED_DIR=<installed-root>" "-DVCPKG_TARGET_TRIPLET=<installed-triplet>" `
    -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF `
    -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/mimalloc-existing --config Release --parallel
```

Replace both placeholders with your existing installation values. The root contains triplet directories; it is not the triplet directory itself. The selected SDK must already be installed with matching headers/libraries and explicit-only allocation behavior. Use a fresh build directory when changing toolchains or triplets. A different manifest against a shared installed root can remove packages absent from that manifest; the reuse command avoids that installation step.

## Linux/macOS SDK setup

Reuse an existing compatible SDK by supplying its prefix:

```sh
cmake -S . -B build/mimalloc -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<allocator-install-prefix>" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/mimalloc --parallel
ctest --test-dir build/mimalloc -C Release --output-on-failure
```

For jemalloc, change the backend to `jemalloc` and use a separate directory. A default system jemalloc package may lack the required explicit API. On Unix, UniMemory also requires the target toolchain's `nm` to check that the library does not replace global C++ allocation.

If you need to build SDKs from source, obtain mimalloc 3.4.3 and jemalloc 5.3.1 from their official release sources. From the UniMemory checkout, with sources extracted into `build/mimalloc` and `build/jemalloc-5.3.1` respectively:

```sh
cmake -S build/mimalloc -B build/mi -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DMI_BUILD_TESTS=OFF "-DCMAKE_INSTALL_PREFIX=$PWD/build/prefix"
cmake --build build/mi --parallel
cmake --install build/mi

(cd build/jemalloc-5.3.1 && ./configure --with-jemalloc-prefix=je_ --disable-cxx --prefix="$PWD/../prefix" && make -j4 && make install)

cmake -S . -B build/jemalloc -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$PWD/build/prefix" -DUNIMEMORY_BACKEND=jemalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/jemalloc --parallel
```

SDK source releases: [mimalloc 3.4.3](https://github.com/microsoft/mimalloc/releases/tag/v3.4.3), [jemalloc 5.3.1](https://github.com/jemalloc/jemalloc/releases/tag/5.3.1). Jemalloc requires a working Unix `make` toolchain; its [official build options](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md) describe platform configuration.

Build mimalloc with `MI_OVERRIDE=OFF`; on Windows also set `MI_WIN_REDIRECT=OFF`. Build jemalloc with both `--with-jemalloc-prefix=je_` and `--disable-cxx`: a prefix alone can still replace global C++ `new/delete`, and UniMemory rejects such Unix libraries. Do not combine explicit UniMemory allocation with a globally overriding SDK build.

## Runtime configuration

Application code includes `<unimem/all.h>` and constructs contexts directly:

```cpp
#include <unimem/all.h>

#include <cstddef>

int main() {
  unimem::Global global;
  unimem::Heap heap;
  alignas(64) std::byte buffer[4096];
  unimem::Stack stack(buffer, sizeof buffer);
  auto value = heap.make_unique<int>(42);
  return *value == 42 ? 0 : 1;
}
```

Statistics default to `Disabled` on all three types. Building with `UNIMEMORY_STATISTICS=ON` permits explicit `StatisticsMode::Basic` at construction; it does not switch it on automatically. An `OFF` build rejects Basic. Keep one backend/statistics/check-policy profile across all translation units, plugins and installed consumers.

Native allocator environment settings must be established before process startup. For a mimalloc DLL built with CRT redirection, use `MIMALLOC_DISABLE_REDIRECT=1` when explicit-only behavior is required; prefer an SDK built with redirection disabled. UniMemory does not change process environment variables at runtime. SDK-native counters and tuning remain separate from UniMemory's requested-byte statistics.

## Install and deploy Release applications

After building, install UniMemory using the same directory and profile:

```sh
cmake --install build/mimalloc --config Release --prefix build/installed
```

In a separate application directory, save the complete runtime sample above as `main.cpp` and create this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(BackendApp LANGUAGES CXX)
find_package(UniMemory CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

From that application directory, replace the placeholders with absolute installation paths. Its configuration must find both UniMemory and the SDK:

```sh
cmake -S . -B build/app -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<UniMemory-install-prefix>;<SDK-install-prefix>"
cmake --build build/app --config Release
```

On Windows, pass the same vcpkg toolchain/triplet when configuring the consumer, or include the existing triplet prefix containing SDK CMake packages. An installed package preserves its backend, statistics and checks policy and rejects conflicts. Application sources only need `<unimem/all.h>`; do not include SDK headers merely to use UniMemory.

| Platform | Dynamically linked SDK deployment |
| --- | --- |
| Windows | Put Release SDK DLLs beside the executable or on its DLL search path; vcpkg Release DLLs are under `<installed-root>/<triplet>/bin` |
| Linux | Deploy SDK `.so` files and configure the application's runtime library search path |
| macOS | Deploy SDK dylibs with matching install names and rpaths |

For custom Unix layouts, set the application's `CMAKE_INSTALL_RPATH` to the deployed SDK location. UniMemory itself has no runtime library to deploy. A statically linked SDK does not need a separate SDK DLL/shared library. Use matching target architecture and compiler/CRT configurations. Jemalloc discovery uses `lib` for Release and `debug/lib` for Debug when present; otherwise Debug falls back to the discovered Release library, so ensure that fallback is compatible with your toolchain.
