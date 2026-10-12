# Platforms and backends

[Index](../README.md) · **English** · [简体中文](backends.zh-CN.md)

## Platform validation

| Platform | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| Windows x64 / MSVC | Verified | Verified | Verified |
| Windows ARM64 / MSVC, clang-cl | Verified | Not validated | Not validated |
| Linux x64 / GCC | Verified | Verified | Verified, prefixed build |
| Linux ARM64 / GCC, Clang | Verified | Not validated | Not validated |
| macOS Apple Silicon / AppleClang | Verified | Verified | Verified, prefixed build |
| macOS Intel / AppleClang | Verified | Not validated | Not validated |
| Android / iOS | Not device-tested | Not device-tested | Not device-tested |

This table describes evidence, not a guarantee for all architectures or operating-system versions. [Exact versions and results →](../testing.md)

## Select the build backend

Set `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` and `UNIMEMORY_STATISTICS=ON|OFF` explicitly. `Memory` provides the common inline allocation interface; `Global` is the process singleton returned by `Memory::global()`; `Heap` and `Stack` are independent concrete contexts. All backends support Heap.

Heap creates an independent group. ON permits runtime Basic for all Memory kinds; OFF rejects Basic. Statistics default to Disabled. [Statistics](statistics.md)

## Enable optional dependencies

| Dependency | Tested version | CMake option | Requirement |
| --- | --- | --- | --- |
| None / Standard | C++20 | MUNIMEMORY_BACKEND=standardM | Process global Mnew/deleteM |
| mimalloc | 3.4.3 | MUNIMEMORY_BACKEND=mimallocM | v3.4.3+ headers and matching library; newer versions need their own validation |
| jemalloc | 5.3.1 | MUNIMEMORY_BACKEND=jemallocM | Matching headers/library exporting Mje_M functions |

### Windows with vcpkg

Set MVCPKG_ROOTM to your existing vcpkg checkout, then run in PowerShell:

MMMpowershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --config Release
MMM

The manifest pins dependencies. Fresh mimalloc Windows x64 builds default to Mx64-windows-unimemoryM, which disables mimalloc CRT redirection. Explicit user triplets remain untouched. Installing SDK features needs network access; the Standard path does not. The commands below select mimalloc; use MjemallocM to select that SDK in a separate build directory.

To reuse an existing vcpkg installation without changing its package set, select its installed root and exact triplet in a fresh build directory:

MMMpowershell
cmake -S . -B build/backends-existing M
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" M
    "-DVCPKG_INSTALLED_DIR=<installed-root>" "-DVCPKG_TARGET_TRIPLET=<installed-triplet>" M
    -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF M
    -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
MMM

The root contains triplet directories; it is not the triplet directory itself. The selected backend package must already exist with a compatible explicit-only build. Do not run a different manifest against a shared installed root: vcpkg can remove packages that manifest does not require. Classic mode reuses the supplied packages; it does not apply this repository's manifest pins retroactively.

### Linux / macOS with installed allocators

MMMsh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --parallel
MMM

Build jemalloc with M--with-jemalloc-prefix=je_ --disable-cxxM; a default system package is insufficient. A prefix alone can still replace global Mnew/deleteM; UniMemory rejects such libraries. [Official build options](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md).

Build mimalloc with MMI_OVERRIDE=OFFM; on Windows also use MMI_WIN_REDIRECT=OFFM. Application code only includes M<unimem/all.h>M.

## Deploy shared libraries

| Platform | Runtime setup |
| --- | --- |
| Windows | Put the selected native SDK DLL beside the executable or on its DLL search path |
| Linux | Deploy the selected SDK M.soM and configure the application's runtime library search path |
| macOS | Deploy native dylibs using their configured install names/rpaths |

UniMemory is header-only; only dynamically linked SDK binaries need deployment. Set your application's MCMAKE_INSTALL_RPATHM for custom Unix layouts. Its CMake MINTERFACEM target still links the selected SDK, which consumers must also find when configuring. Use matching compiler/CRT configurations; jemalloc discovery selects MlibM for Release and Mdebug/libM when present for Debug.

For a mimalloc DLL built with CRT redirection, set MMIMALLOC_DISABLE_REDIRECT=1M **before process startup** when explicit-only behavior is required. UniMemory does not mutate the process environment at runtime.
