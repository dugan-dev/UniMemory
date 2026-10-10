# Upstream validation

Recorded runs below precede the header-only fixed-profile migration. Static/shared UniMemory references describe those historical builds; current builds use a CMake INTERFACE target with a separately linked SDK. See [current configuration](getting-started.md).

[Documentation](README.md) · [Tests](testing.md)

UniMemory tests its own contract and runs upstream suites separately. An upstream test exercises the allocator's native API; it does not prove that the UniMemory adapter is correct.

## UniMemory scenarios

The adapter suite includes 240 loop cases per backend and 240 Stack cases, 414 interface-use cases, 352 ownership traces of 1024 steps, 192 allocation traces of 4096 steps and three 250000-step runs. Coverage includes factory startup, object/array cleanup, allocator propagation, adoption, shared/weak ownership, marks, reset failure, native large-block release accounting and synchronized handoff.

Local WSL ThreadSanitizer uses `setarch x86_64 -R` for the test process and its children to avoid an initial mapping error. No system setting is changed. The Standard/Stack sanitizer runs do not instrument native allocator internals.

## Fixed sources

| Suite | Revision | Execution |
| --- | --- | --- |
| [mimalloc](https://github.com/microsoft/mimalloc/blob/v3.4.3/CMakeLists.txt) | 3.4.3; `152fbf2634aeafca3774df791b0a77683035f076` | All four upstream CTest programs; native allocation, fill and stress |
| [jemalloc](https://github.com/jemalloc/jemalloc/blob/5.3.1/Makefile.in) | 5.3.1; `81034ce1f1373e37dc865038e1bc8eeecf559ce8` | `make check`, `make stress`, `make analyze`; profiling enabled |
| [mimalloc-bench](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86) | `ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86` | All 16 packaged allocation/application programs listed below |
| [rpmalloc](https://github.com/mjansson/rpmalloc/tree/1.4.5/test) | 1.4.5; `e4393ff85585d91400bcbad2e7266c011075b673` | Native C suite with assertions/statistics/heaps; C++ override suite with upstream library defaults; test-only dependency |

Validated allocator sources are pinned. Upstream sources stay in ignored build directories; their original licenses are preserved. Test tools are not installed with the library.

Unix explicit jemalloc uses `--with-jemalloc-prefix=je_ --disable-cxx`. This preserves the program's normal C++ new/delete path. The explicit profile excludes native C++ replacement tests; it does not restrict UniMemory Object or Container operations.

Native suite results are separate from the current UniMemory adapter results. Rerun the scripts for your platform and dependency configuration.

The jemalloc 5.3.1 profiling suite uses the upstream [extent-test fix](https://github.com/jemalloc/jemalloc/pull/2954), pinned to `1b022c0da70c0d9d259e9beab6fb7db91ae79567` with SHA-256 verification. The test handles permitted profiling-related `xallocx` growth refusal and adds a page-aligned case to retain commit/merge checks. Only the test is backported; allocator implementation and installed libraries remain 5.3.1. `tools/patch-jemalloc-tests.py` applies the fix before validation.

## Reproduce

Linux, with CMake, a C/C++ compiler, Make, Git, Python 3, curl and GNU time installed:

```sh
bash tools/validate-upstream.sh "$HOME/unimemory-validation"
bash tools/validate-packaging.sh "$HOME/unimemory-validation"
bash tools/validate-benchmark-collection.sh "$HOME/unimemory-validation"
bash tools/validate-rpmalloc.sh "$HOME/rpmalloc-validation"
```

## External workloads

| Family | Programs | What they exercise |
| --- | --- | --- |
| Applications | `cfrac`, `espresso`, `barnes` | Allocation patterns in numerical and logic programs |
| Thread handoff | `larson`, `larson-sized`, `xmalloc-test`, `mstress`, `mleak` | Mixed lifetimes, cross-thread frees and cache retention |
| Cache contention | `cache-scratch`, `cache-thrash` | Sharing and false sharing |
| Large allocations | `malloc-large`, `malloc-large-old` | Repeated 5–25 MiB blocks |
| Synthetic traces | `alloc-test`, `rptest` | Size distributions and concurrent allocation; `rptest` originates in rpmalloc |
| libc benchmarks | `glibc-simple`, `glibc-thread` | Single-thread and multi-thread malloc loops |

Three shuffled trials per program/backend, fresh processes, four threads where configurable. The runner records exact arguments, exit codes, elapsed/user/system time and peak RSS. It uses separate native override builds of mimalloc and jemalloc via Linux `LD_PRELOAD`. **These measurements compare native process allocators.** Measure the UniMemory layer separately with `UniMemoryDeliveryBenchmark`.

## Scope and exclusions

| Item | Treatment |
| --- | --- |
| Conditional upstream skips | Record separately from passes; retain the complete logs |
| mimalloc `test-stress-dynamic` with `MI_OVERRIDE=OFF` | Exercises the normal process allocation path; do not count it as an override validation |
| `sh6bench`, `sh8bench` | Their licensed source files are absent from the upstream repository; not built or counted |
| Security probes | Intentionally execute use-after-free, double-free or corruption; not valid portable API tests |
| Extra applications such as Redis, Lean and RocksDB | Additional dependency graphs; outside this validation suite |
| Android/iOS | Not device-tested in this release |
| GNU `--wrap=mi_heap_new` | The reset-failure fixture requires static UniMemory; the linker cannot wrap calls inside an existing shared library |
| Windows `new` replacement | Allocation-failure injection requires static UniMemory; it cannot replace DLL-internal calls |

No allocation library can convert invalid pointer lifetimes, mismatched frees or concurrent Heap `reset()` into defined behavior without additional mechanisms. UniMemory documents these preconditions and validates legal calls plus specified failure behavior.
