# Platforms and backends

[Index](../README.md) · **English** · [简体中文](backends.zh-CN.md)

## Platform validation

| Platform | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| Windows x64 / MSVC | Verified | Verified | Verified |
| Linux x64 / GCC | Verified | Verified | Verified, prefixed build |
| macOS / Apple Clang | GitHub CI | GitHub CI | GitHub CI, prefixed build |
| Android / iOS | Not device-tested | Not device-tested | Not device-tested |

This table describes evidence, not a guarantee for all architectures or operating-system versions. [Exact versions and results →](../testing.md)

## Select an enabled backend

Set `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` and `UNIMEMORY_STATISTICS=ON|OFF` explicitly when configuring. One build contains one selected allocation backend; `Memory` remains a non-template class. `Memory::global()` returns its shared Global instance. Explicit `global(backend)` and `heap(backend)` must match `Memory::selected_backend`, otherwise they throw `std::invalid_argument`; there is no fallback. `available(backend)` is true only for that selected backend, including when Standard is not selected.

Heap creates an independent group where supported. Statistics ON fixes Global Basic and permits Heap Disabled/Basic; OFF fixes Global Disabled and rejects Heap Basic. Stack has no backend or statistics. [Statistics](statistics.md)

## Enable optional dependencies

| Dependency | Tested version | CMake option | Requirement |
| --- | --- | --- | --- |
| None / Standard | C++20 | `UNIMEMORY_BACKEND=standard` | Process global `new/delete` |
| mimalloc | 3.4.3 | `UNIMEMORY_BACKEND=mimalloc` | v3.4.3+ headers and matching library; newer versions need their own validation |
| jemalloc | 5.3.1 | `UNIMEMORY_BACKEND=jemalloc` | Matching headers/library exporting `je_` functions |

### Windows with vcpkg

Set `VCPKG_ROOT` to your existing vcpkg checkout, then run in PowerShell:

```powershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --config Release
```

The manifest pins dependencies. Fresh mimalloc Windows x64 builds default to `x64-windows-unimemory`, which disables mimalloc CRT redirection. Explicit user triplets remain untouched. Installing SDK features needs network access; the Standard path does not. The commands below select mimalloc; use `jemalloc` to select that SDK in a separate build directory.

To reuse an existing vcpkg installation without changing its package set, select its installed root and exact triplet in a fresh build directory:

```powershell
cmake -S . -B build/backends-existing `
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_INSTALLED_DIR=<installed-root>" "-DVCPKG_TARGET_TRIPLET=<installed-triplet>" `
    -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF `
    -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
```

The root contains triplet directories; it is not the triplet directory itself. The selected backend package must already exist with a compatible explicit-only build. Do not run a different manifest against a shared installed root: vcpkg can remove packages that manifest does not require. Classic mode reuses the supplied packages; it does not apply this repository's manifest pins retroactively.

### Linux / macOS with installed allocators

```sh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --parallel
```

Build jemalloc with `--with-jemalloc-prefix=je_ --disable-cxx`; a default system package is insufficient. A prefix alone can still replace global `new/delete`; UniMemory rejects such libraries. [Official build options](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md).

Build mimalloc with `MI_OVERRIDE=OFF`; on Windows also use `MI_WIN_REDIRECT=OFF`. Application code only includes `<unimem/memory.h>`.

## Deploy shared libraries

| Platform | Runtime setup |
| --- | --- |
| Windows | Put the selected native SDK DLL beside the executable or on its DLL search path |
| Linux | Deploy the selected SDK `.so` and configure the application's runtime library search path |
| macOS | Deploy native dylibs using their configured install names/rpaths |

UniMemory is header-only; only dynamically linked SDK binaries need deployment. Set your application's `CMAKE_INSTALL_RPATH` for custom Unix layouts. Its CMake `INTERFACE` target still links the selected SDK, which consumers must also find when configuring. Use matching compiler/CRT configurations; jemalloc discovery selects `lib` for Release and `debug/lib` when present for Debug.

For a mimalloc DLL built with CRT redirection, set `MIMALLOC_DISABLE_REDIRECT=1` **before process startup** when explicit-only behavior is required. UniMemory does not mutate the process environment at runtime.
