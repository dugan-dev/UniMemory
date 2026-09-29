# Tests

[Documentation](README.md) · **English** · [简体中文](testing.zh-CN.md)

## 0.0.1 verification

| Build | Result |
| --- | --- |
| Windows x64 / MSVC, three backends | Release **1626/1626**, Debug **1626/1626** |
| Linux x64 / GCC / WSL, three backends | **1627/1627** |
| Linux Standard / Stack, ASan + UBSan | **681/681**, no sanitizer findings |
| Linux Standard / Stack, ThreadSanitizer | **678/678**, no race findings; examples disabled |
| Linux Standard linked to Google TCMalloc | **671/671** adapter tests |
| Installed package, Windows and Linux | Exact `0.0.1` lookup, version macros and consumer pass |
| Independent GitHub clone | README build, Standard **681/681**; static/shared installed consumers **2/2** each |
| Documentation examples | 31 fragments compile; Standard paths run |

Backend versions: mimalloc 3.4.3 and jemalloc 5.3.1. These are local results for the unified Memory interface.

## Coverage

| Topic | Checks |
| --- | --- |
| Allocation | Zero size, alignment, overflow, allocation failure |
| Object / Array | Forwarded construction, exact-type destruction, reverse rollback, adoption, Shared/Weak/alias ownership |
| Container | Allocator rebind/propagation, PMR, copy/move/swap, cross-Memory use |
| Resize | Content preservation, zeroed growth, failure rollback |
| Heap / Stack | Ownership, exclusive controls, reset rollback, foreign/stale/nested marks, constructor failure |
| Global / Threads | Concurrent configuration/lookup, shared identity, static destruction, synchronized Object read/write and ownership handoff |
| Statistics | Startup configuration, exact concurrent totals, unavailable native fields, native large-block release regression |
| Stress | 240 loop cases per backend + 240 Stack cases; 352 ownership traces × 1024 steps, 192 allocation traces × 4096 steps, three 250000-step runs |
| Invalid use | Unbound nonempty Deleter, zero Array count, stale marks, overflow, unavailable capabilities |
| Header entry point | One `unimem/memory.h` include provides Memory, Block, Allocator and smart-pointer helpers |

The 414 interface-use cases cover Global/Heap, counters on/off and Stack.
Thread safety is tested with valid synchronized access; arbitrary invalid pointers,
unsynchronized Object writes and use after reset remain caller errors.
Shared-array exception order has a reproduced libstdc++ 13 deviation;
[the compatibility guide](compatibility.md#standard-library-behavior) describes the exact test condition.

## Run

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

Standard is enabled by default; optional backends need their build options and dependencies. [Configuration](guides/backends.md)

## Native validation

[Upstream suites and reproduction](upstream-validation.md) · [Performance](performance.md)

macOS and Android/iOS are not verified for this build. Test results describe the checked configurations and workloads.

For local WSL ThreadSanitizer, `setarch x86_64 -R` disables address randomization
only for the test process and its children to avoid the host's initial mapping error.
No system setting was changed. Native allocator internals are not instrumented by this Standard/Stack run.

The results above are local checks. The checked-in workflows provide reproducible CI configurations.
