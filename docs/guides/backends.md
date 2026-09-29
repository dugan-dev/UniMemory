# Backends and platforms

[Index](../README.md) · **English** · [简体中文](backends.zh-CN.md)

## Select an enabled backend

```cpp
unimem::Memory& standard = unimem::Memory::global();
if (unimem::available(unimem::Backend::Mimalloc)) {
    unimem::Memory& selected = unimem::Memory::global(unimem::Backend::Mimalloc);
    unimem::OwnedBlock block = selected.make_block(1024);
}
```

The backend is fixed for the instance's lifetime. An unavailable backend throws; there is no silent fallback. Global returns one shared Memory per Backend; heap creates an independent Heap with optional separate counters.

## Enable optional dependencies

| Dependency | Tested version | CMake option | Requirement |
| --- | --- | --- | --- |
| None / Standard | C++20 | Default | Process global `new/delete` |
| mimalloc | 3.4.3 | `UNIMEMORY_WITH_MIMALLOC=ON` | v3.4.3+ headers and matching library; newer versions need their own validation |
| jemalloc | 5.3.1 | `UNIMEMORY_WITH_JEMALLOC=ON` | Matching headers/library exporting `je_` functions |
| Google TCMalloc | Pinned revision in report | Final executable linkage | Linux; changes process allocation, not Backend enum |

### Windows with vcpkg

Set `VCPKG_ROOT` to your existing vcpkg checkout, then run in PowerShell:

```powershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON
cmake --build build/backends --config Release
ctest --test-dir build/backends -C Release --output-on-failure
```

The manifest pins dependencies. Fresh Windows x64 builds default to `x64-windows-unimemory`, which disables mimalloc CRT redirection. Explicit user triplets remain untouched. Installing features needs network access; the Standard path does not.

### Linux / macOS with installed allocators

```sh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON
cmake --build build/backends --parallel
ctest --test-dir build/backends --output-on-failure
```

Build jemalloc with `--with-jemalloc-prefix=je_ --disable-cxx`; an ordinary system package is insufficient. The prefix alone still allows global C++ `new/delete` replacement. Unix configuration inspects symbols and rejects that build, including installed-package consumers. Disabling this native replacement does not affect UniMemory's object APIs. [Official build options](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md).

Build mimalloc with `MI_OVERRIDE=OFF`; on Windows also use `MI_WIN_REDIRECT=OFF`. The [upstream reproduction script](../upstream-validation.md#reproduce) builds the tested Unix packages. Backend headers never appear in application code.

## Platform validation

| Platform | Standard | mimalloc | jemalloc | Google TCMalloc |
| --- | --- | --- | --- | --- |
| Windows x64 / MSVC | Final local tests | Final local tests | Final local tests | Outside upstream's listed support |
| Linux x64 / GCC | Final local tests | Final local tests | Final local prefixed build | Final local WSL linkage and suite |
| macOS | Not verified | Not verified | Not verified | Outside upstream's listed support |
| Android / iOS | Not device-tested | Not device-tested | Not device-tested | Outside upstream's listed support |

This table describes evidence, not a guarantee for all architectures or operating-system versions. [Exact versions and results →](../testing.md)

## Deploy shared libraries

| Platform | Runtime setup |
| --- | --- |
| Windows | Put UniMemory and required native DLLs beside the executable or on its DLL search path |
| Linux | Private installations default to `$ORIGIN`; native `.so` files can sit beside `libUniMemory.so` |
| macOS | Deploy native dylibs using their configured install names/rpaths |

Override custom Unix layouts using `CMAKE_INSTALL_RPATH`. Consumers must find optional native packages when configuring. Use matching compiler/CRT configurations; jemalloc discovery selects `lib` for Release and `debug/lib` when present for Debug.

For a mimalloc DLL built with CRT redirection, set `MIMALLOC_DISABLE_REDIRECT=1` **before process startup** when explicit-only behavior is required. UniMemory does not mutate the process environment at runtime.
