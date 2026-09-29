# Google TCMalloc linkage

[Index](../README.md) · **English** · [简体中文](tcmalloc.zh-CN.md)

Google TCMalloc changes the final program's allocation path. It is distinct from gperftools' TCMalloc. UniMemory does not offer `Backend::TCMalloc`; linked TCMalloc can supply the Standard global new/delete path.

| Native feature | Scope | UniMemory |
| --- | --- | --- |
| C/C++ allocation, alignment and sized deallocation | Final executable/process | Standard path after final linkage |
| Per-CPU / per-thread caches | Native allocator | Internal policy, not a UniMemory Heap |
| MallocExtension statistics and sampling | Process | Not a unified Memory backend snapshot |
| Reclamation and cache limits | Process | No per-Memory runtime control |
| Guarded sampling | Native diagnostic mode | Not a universal error detector |

The upstream platform list covers supported 64-bit Linux combinations. Windows/macOS/mobile are outside that listed scope. Bazel is the validated route here; do not infer CMake package validation from the linkage helper.

`unimemory_link_tcmalloc(executable, allocator_target)` accepts existing CMake targets on Linux. Local validation used Bazel to link UniMemory's static library into a final program; the helper itself was not tested with a real native CMake target.

Tested revision: `1c6a831d649134efac38663f5a269a43f0d02702`. [Results and WSL clock conditions](../testing.md) · [Reproduce](../upstream-validation.md)

Sources: [platforms](https://google.github.io/tcmalloc/platforms.html), [reference](https://google.github.io/tcmalloc/reference.html), [pinned MallocExtension header](https://github.com/google/tcmalloc/blob/1c6a831d649134efac38663f5a269a43f0d02702/tcmalloc/malloc_extension.h).
