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

Sources: [official manual](https://jemalloc.net/jemalloc.3.html), [5.3.1 public header](https://github.com/jemalloc/jemalloc/blob/5.3.1/include/jemalloc/jemalloc.h.in), [validation](../upstream-validation.md). Platform evidence: [test results](../testing.md).
