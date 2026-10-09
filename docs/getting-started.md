# Build and install

[Index](README.md) · **English** · [简体中文](getting-started.zh-CN.md)

## Run an example

Follow the [README build steps](../README.md#build-and-test), then run the basic example:

| Platform | Executable |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS | `build/UniMemory-release/UniMemoryExample_basic` |

Expected output: `3, 1024 bytes`. [More examples](../examples/README.md)

## Add UniMemory to a project

Choose [source or installed-package integration](../README.md#cmake-integration). Linking `UniMemory::UniMemory` provides include paths and C++20 settings. Include `<unimem/memory.h>` in your code.

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

Replace the placeholder with the absolute installation path. Your project's CMake file must use the [installed-package target](../README.md#cmake-integration).

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | OFF unless set by your project | Build a shared library |
| `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC` | OFF | Enable optional backends |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | Build runnable examples |

For a library-only build, set `-DBUILD_TESTING=OFF`.

## Deploy your application

CMake must also find any enabled native backend dependencies. For shared builds, deploy UniMemory and the required backend libraries with your application. See [backend setup](guides/backends.md#deploy-shared-libraries).

[Object ownership](guides/objects.md) · [Containers](guides/containers.md) · [Raw memory](guides/raw-memory.md)
