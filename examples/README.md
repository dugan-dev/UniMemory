# Examples

[Home](../README.md) · [Getting started](../docs/getting-started.md)

| Program | Demonstrates |
| --- | --- |
| [basic.cpp](basic.cpp) | Objects, aligned blocks and PMR containers |
| [heap.cpp](heap.cpp) | Capability checks, ownership, collect and reset |
| [scratch.cpp](scratch.cpp) | Fixed-buffer allocation and rewind |

```sh
cmake --preset release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
```

Run an executable from the repository root:

| Program | Windows / Visual Studio | Linux / macOS |
| --- | --- | --- |
| Basic | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` | `build/UniMemory-release/UniMemoryExample_basic` |
| Heap | `build/UniMemory-release/Release/UniMemoryExample_heap.exe` | `build/UniMemory-release/UniMemoryExample_heap` |
| Stack | `build/UniMemory-release/Release/UniMemoryExample_scratch.exe` | `build/UniMemory-release/UniMemoryExample_scratch` |

Heap uses the selected mimalloc or jemalloc backend and skips Standard, which has no independent Heap. Examples are optional and are not installed with the library.
