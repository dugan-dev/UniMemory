# Examples

[Home](../README.md) · [Getting started](../docs/getting-started.md)

| Program | Demonstrates |
| --- | --- |
| [basic.cpp](basic.cpp) | Objects, aligned blocks and PMR containers |
| [heap.cpp](heap.cpp) | Capability checks, ownership, collect and reset |
| [scratch.cpp](scratch.cpp) | Fixed-buffer allocation and rewind |

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release -R UniMemory.example
```

The Heap example runs each enabled optional backend. With Standard only, it skips unsupported Heaps. Examples are optional and are not installed with the library.
