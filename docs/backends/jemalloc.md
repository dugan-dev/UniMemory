# jemalloc mapping

[Index](../README.md) · **English** · [简体中文](jemalloc.zh-CN.md)

Tested: **5.3.1** with explicit `je_` exports. Enable `UNIMEMORY_WITH_JEMALLOC`; Unix packages must use `--with-jemalloc-prefix=je_ --disable-cxx`. The prefix does not disable global C++ operators; UniMemory rejects libraries that export those replacements. [Official 5.3.1 build options](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md). The online manual describes **5.4.0** and cannot prove that a 5.3.1 build has every new symbol.

| Native feature | Key functions / controls | UniMemory decision |
| --- | --- | --- |
| Allocation / alignment / zeroing | `je_mallocx`, ALIGN/ZERO flags | Common byte APIs |
| Resize | `je_rallocx`, `je_xallocx` | Reallocate contract; in-place xallocx not exposed |
| Sizes / sized free | `je_sallocx`, `je_nallocx`, `je_sdallocx` | Not public; size parameter retained, current free uses dallocx |
| Independent region | `arenas.create`, arena allocation/reset/destroy | Memory::heap(Backend::Jemalloc), per-group tcache disabled |
| Membership / reclaim | `arenas.lookup`, arena purge | Heap owns/collect |
| Thread caches / extent hooks | tcache controls, extent callbacks | Not public |
| Controls | `je_mallctl` and MIB variants | Uniform reclaim-delay defaults only |
| Statistics | epoch refresh, stats controls | Optional allocated/resident metrics |
| Sampling profiling | prof controls | Not public |

Detailed statistics depend on the native statistics build. Query capabilities; every returned metric remains optional. Native snapshots may synchronize. Ordinary Memory uses the default shared `je_` path; only Memory::heap() creates an explicit region.

An installed package retains its jemalloc dependency. If discovery or symbol validation fails, `find_package(UniMemory CONFIG QUIET)` returns `UniMemory_FOUND=FALSE` with `UniMemory_NOT_FOUND_MESSAGE` and does not import UniMemory or jemalloc targets. `find_package(... REQUIRED)` and explicitly enabling the source backend still fail configuration. Windows keeps distinct release/debug libraries, with Release as the fallback when no debug library is available.

## Initialization

Global/Heap creation, capability queries and runtime-option changes share a first-use initialization guard. It completes a native version query before concurrent UniMemory operations can enter jemalloc. Initialization failure remains retryable: factories and runtime-option changes report failure by exception; noexcept capability lookup reports no detailed statistics. `available()` describes compiled support, not a successful initialization probe.

The guard is per linked UniMemory copy. Raw `je_*` calls and independent wrapper copies sharing a native DLL require coordinated startup outside this guard. It does not replace synchronization for object access or exclusive Heap reclamation. [Cold-start evidence](../review-2026-10-08.md#concurrent-jemalloc-initialization)

Sources: [official manual](https://jemalloc.net/jemalloc.3.html), [5.3.1 public declarations](https://github.com/jemalloc/jemalloc/blob/5.3.1/include/jemalloc/jemalloc_protos.h.in), [validation](../upstream-validation.md). Platform evidence: [test results](../testing.md).
