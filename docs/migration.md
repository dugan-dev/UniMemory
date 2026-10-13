# Header-only configuration migration

[Index](README.md) · **English** · [简体中文](migration.zh-CN.md)

`Memory` is a non-template base with shared non-virtual allocation functions; `Global`, `Heap` and `Stack` are final subclasses. Include `<unimem/all.h>` and link `UniMemory::UniMemory`; the target now carries inline implementation, generated configuration and the selected SDK dependency instead of a UniMemory binary.

`Memory::global()` becomes a caller-owned `Global memory;`; `Memory::heap()` becomes `Heap heap;`, and `Memory::stack(buffer)` becomes `Stack scratch(buffer);`. Use `Stack::Mark` for buffer checkpoints. Common allocation and ownership code keeps `Memory&`; Heap/Stack controls require their concrete type.

Own Global, Heap and Stack through their concrete types, such as `std::unique_ptr<Heap>`; use `Memory&` or `Memory*` as a borrowed view. Deletion through `Memory*` is disabled.

| Earlier usage | Current usage |
| --- | --- |
| `<unimem/memory.h>` as the complete entry | `<unimem/all.h>` aggregates all declarations and inline implementations; `memory.h` declares `Memory`; `global.h`, `heap.h` and `stack.h` declare their respective classes |
| Optional backend enable flags / runtime backend selection | Explicit `UNIMEMORY_BACKEND=standard`, `mimalloc` or `jemalloc`, one per build |
| `backend_statistics()` / `BackendStatistics` | Removed; `statistics()` provides five request counter fields |
| `global(backend)` / `heap(backend, statistics)` | `Global(statistics)` / `Heap(statistics)` |
| `capabilities(...)` / `memory.capabilities()` | Removed; use the Memory kind and selected Backend contracts |
| `supports(...)` / `set_runtime_option(...)` | `Memory::config(release_delay_ms)`; valid values are a no-op for Standard |
| `available(backend)` / `BackendCapabilities::available` | Removed; CMake selects the backend once |
| `Memory::selected_backend` / `selected_statistics` | Removed; use CMake `UNIMEMORY_BACKEND` / `UNIMEMORY_STATISTICS` |
| `Memory::global()` / `Memory::heap()` / `Memory::stack(...)` | Removed; directly construct `Global`, `Heap` and `Stack`, with caller-managed lifetimes |
| `Memory::configure_global` | Select statistics per instance through constructors or `set_statistics()`; Basic requires ON support |
| Singleton Global with shared statistics | Pass Basic to each Global constructor; ON permits it, OFF rejects it |
| Heap Basic in any build | Basic requires ON; OFF rejects it with `invalid_argument` |
| Default Global uses Standard | `Global()` uses the CMake-selected backend |
| Always-checked allocation preconditions | `UNIMEMORY_CHECKS=AUTO`: Debug ON, other configurations OFF; explicit ON/OFF supported |

Public backend identifiers and queries are removed. CMake selects the backend. Pass statistics to each Global/Heap/Stack constructor; all modes default to Disabled.

Use separate source build and installation directories for each backend/statistics/check-policy profile. An installed package exports its fixed configuration; consumers inherit it and must not mix profiles or override conflicting macros across translation units. Checks OFF does not remove allocation failure, typed overflow, constructor rollback or configuration mismatch behavior; valid pointer, alignment, count and lifetime contracts still apply.

[Build options](getting-started.md#build-options) · [API](api-reference.md) · [Lifetime](compatibility.md)
