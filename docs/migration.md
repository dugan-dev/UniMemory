# Header-only configuration migration

[Index](README.md) · **English** · [简体中文](migration.zh-CN.md)

`Memory` remains a non-template class. Include `<unimem/memory.h>` and link `UniMemory::UniMemory`; the target now carries inline implementation, generated configuration and the selected SDK dependency instead of a UniMemory binary.

| Earlier usage | Current usage |
| --- | --- |
| Optional backend enable flags / runtime backend selection | Explicit `UNIMEMORY_BACKEND=standard`, `mimalloc` or `jemalloc`, one per build |
| `Memory::configure_global` | Removed; set `UNIMEMORY_STATISTICS=ON/OFF` in CMake |
| Global Basic chosen before first lookup | ON fixes Global Basic; OFF fixes Global Disabled |
| Heap Basic in any build | Basic requires ON; OFF rejects it with `invalid_argument` |
| Default Global uses Standard | `global()` uses `Memory::selected_backend` |
| Always-checked allocation preconditions | `UNIMEMORY_CHECKS=AUTO`: Debug ON, other configurations OFF; explicit ON/OFF supported |

Explicit `global(backend)` and `heap(backend)` still exist, but a different backend throws `invalid_argument`. `available()` is true only for the selected backend. Stack remains independent of backend/statistics.

Use separate source build and installation directories for each backend/statistics/check-policy profile. An installed package exports its fixed configuration; consumers inherit it and must not mix profiles or override conflicting macros across translation units. Checks OFF does not remove allocation failure, typed overflow, constructor rollback or configuration mismatch behavior; valid pointer, alignment, count and lifetime contracts still apply.

[Build options](getting-started.md#build-options) · [API](api-reference.md) · [Lifetime](compatibility.md)
