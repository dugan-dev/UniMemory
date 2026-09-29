# mimalloc mapping

[Index](../README.md) · **English** · [简体中文](mimalloc.zh-CN.md)

Tested: **3.4.3**. UniMemory requires v3.4.3+ for its statistics API, matching headers/library, and `UNIMEMORY_WITH_MIMALLOC=ON`. Newer releases are not automatically validated.

| Native feature | Key functions | UniMemory decision |
| --- | --- | --- |
| Basic allocation | `mi_malloc`, `mi_free`, `mi_realloc` | Ordinary Memory uses default allocation |
| Alignment | `mi_malloc_aligned`, `mi_realloc_aligned` | Uniform alignment parameter |
| Zero initialization | `mi_zalloc`, `mi_rezalloc` | Uniform request-range zeroing; avoid native old-block preconditions |
| Typed / constant-size helpers | Typed macros, `mi_malloc_csize`, `mi_free_csize` | C++ object templates; no promised constant-size fast path |
| Independent heap | `mi_heap_new`, heap allocation, `mi_heap_destroy` | Memory::heap(Backend::Mimalloc) |
| Collect and membership | `mi_heap_collect`, `mi_heap_contains` | Heap collect/owns |
| Detailed statistics | `mi_stats_get` | Process committed/reserved only; independent Heap details unavailable |
| Runtime options | `mi_option_set`, `mi_option_get` | Advisory purge delay only |
| Heap traversal | `mi_heap_visit_blocks` | Not exposed |
| Large OS arenas / subprocesses | Reservation and isolation APIs | Not exposed; different scope from UniMemory Heap |
| Thread-local fast interfaces | Native v3 theap APIs | Not exposed |
| POSIX / C++ wrappers | Native prefixed compatibility functions | Application uses UniMemory owners/adapters |

Windows/Linux final local checks are recorded in the [report](../testing.md). Android/iOS are not device-tested. Disable global override and Windows CRT redirection for explicit-only selection; see [setup](../guides/backends.md).

## Statistics accuracy

In the pinned default Release build, `MI_STAT=0`: normal malloc counters are not
recorded. Requested counters and huge-allocation release updates also do not
consistently represent live bytes. Heap page accounting is recorded at subprocess
scope. UniMemory therefore returns only Process committed/reserved metrics; a
mimalloc Heap reports no native detailed-statistics capability. Enable Basic for
exact request counters on either kind.

Verified against [build conditions](https://github.com/microsoft/mimalloc/blob/v3.4.3/include/mimalloc/types.h#L70),
[allocation counters](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/alloc.c#L57),
[release counters](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/free.c#L563)
and [OS page accounting](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/os.c#L223).
Unavailable fields remain empty optionals. The aggregate covers the calling
thread's current native subprocess, normally main, rather than all native
subprocesses ([query implementation](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/stats.c#L566)).

Sources: [official topics](https://microsoft.github.io/mimalloc/topics.html), [pinned header](https://github.com/microsoft/mimalloc/blob/v3.4.3/include/mimalloc.h), [statistics definitions](https://github.com/microsoft/mimalloc/blob/v3.4.3/include/mimalloc-stats.h). The website may describe newer releases; the pinned sources define the tested symbols.
