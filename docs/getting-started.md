# Build and install

[Index](README.md) · **English** · [简体中文](getting-started.zh-CN.md)

## Get the source

Download the [dev source ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip), or clone the repository:

```sh
git clone --branch dev https://github.com/dugan-dev/UniMemory.git
```

Run the build commands from the directory containing `CMakeLists.txt`. The library version is declared there; use headers, generated configuration and SDK libraries from matching revisions.

## Run an example

Follow the [README build steps](../README.md#build-and-install), then run the basic example:

| Platform | Executable |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS | `build/UniMemory-release/UniMemoryExample_basic` |

Expected output: `3, 1024 bytes`. [More examples](../examples/README.md)

## Add UniMemory to a project

Follow the [project integration example](../README.md#integration). `UniMemory::UniMemory` is a header-only `INTERFACE` target that provides include paths, generated configuration, C++20 settings and the selected SDK link dependency. Include `<unimem/memory.h>` in your code.

## Install the library

Run from the UniMemory checkout after building:

```sh
cmake --install build/UniMemory-release --config Release --prefix build/installed
```

In your own project directory, point CMake to that installation:

```sh
cmake -S . -B build "-DCMAKE_PREFIX_PATH=<UniMemory-install-prefix>"
cmake --build build --config Release
```

Replace the placeholder with the absolute installation path. Your project's CMake file must use the [installed-package target](../README.md#integration).

The README's `release` preset uses `build/UniMemory-release`. If you choose another directory with `cmake -B`, use it consistently for running examples and installation.

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `UNIMEMORY_BACKEND` | Required, no default | Exactly one of `standard`, `mimalloc`, `jemalloc` |
| `UNIMEMORY_STATISTICS` | Required, no default | `ON`: Global Basic and Heap Basic support; `OFF`: Global Disabled, Heap Basic rejected |
| `UNIMEMORY_CHECKS` | `AUTO` | Checks in Debug, omitted in other configurations; `ON`/`OFF` override the policy |
| `UNIMEMORY_AGGRESSIVE_INLINING` | ON | MSVC Release `/Ob3`; OFF preserves consumer inlining settings |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | Build runnable examples |

For a headers/package-only installation, set `-DUNIMEMORY_BUILD_TESTS=OFF`. No UniMemory static or shared binary is produced; `BUILD_SHARED_LIBS` does not choose a UniMemory library form.

For source integration, set the mandatory options before adding the subdirectory:

```cmake
set(UNIMEMORY_BACKEND standard CACHE STRING "UniMemory backend")
set(UNIMEMORY_STATISTICS OFF CACHE BOOL "UniMemory counters")
add_subdirectory(UniMemory)
```

An installed package retains its selected backend/statistics/check policy and generated `<unimem/config.h>`. Consumers normally inherit them from the target. Conflicting requested CMake settings or compile definitions are rejected; use a separate installation/build directory for a different profile and keep one profile across all translation units. See [migration](migration.md).

## Inlining and optimization

`UNIMEMORY_AGGRESSIVE_INLINING=ON` adds `/Ob3` to MSVC Release consumers by default; it does not add that option to Debug or other compilers. It affects the entire C++ translation unit linking the interface target, including code outside UniMemory. Code size and unrelated workload performance can change.

Set `-DUNIMEMORY_AGGRESSIVE_INLINING=OFF` when configuring either a source build or an installed-package consumer to remove the target's `/Ob3` and choose your own inlining settings. The installed target uses a consumer-controlled property; this preference differs from the fixed backend/statistics/check-policy contract. Global IPO/LTO is not enabled automatically.

[Measured controls and limits](performance/header-only.md) keep SDK/API compiler settings matched. `/Ob3` remains a compiler heuristic, not a promise to inline every function or make every workload fastest.

## Deploy your application

CMake must also find the selected native SDK when using the installed package. Deploy a dynamically linked SDK with your application; UniMemory itself has no runtime library to deploy. See [backend setup](guides/backends.md#deploy-shared-libraries).

[Object ownership](guides/objects.md) · [Containers](guides/containers.md) · [Raw memory](guides/raw-memory.md)
