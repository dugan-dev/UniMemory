# Upstream validation

[Documentation](README.md) · [Tests](testing.md)

UniMemory tests its own contract and runs upstream suites separately. An upstream test exercises the allocator's native API; it does not prove that the UniMemory adapter is correct.

## Fixed sources

| Suite | Revision | Execution |
| --- | --- | --- |
| [mimalloc](https://github.com/microsoft/mimalloc/blob/v3.4.3/CMakeLists.txt) | 3.4.3; `152fbf2634aeafca3774df791b0a77683035f076` | All four upstream CTest programs; native allocation, fill and stress |
| [jemalloc](https://github.com/jemalloc/jemalloc/blob/5.3.1/Makefile.in) | 5.3.1; `81034ce1f1373e37dc865038e1bc8eeecf559ce8` | `make check`, `make stress`, `make analyze`; profiling enabled |
| [Google TCMalloc](https://github.com/google/tcmalloc/blob/1c6a831d649134efac38663f5a269a43f0d02702/tcmalloc/testing/BUILD) | `1c6a831d649134efac38663f5a269a43f0d02702` | `bazel test //tcmalloc/...`; native `tcmalloc_benchmark`; final-program linking |
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

TCMalloc requires Bazel 8.4.2. Run `bash tools/validate-tcmalloc.sh "$HOME/tcmalloc-validation"`. Its dependencies are recorded in [release-validation.yml](../.github/workflows/release-validation.yml). `tests/tcmalloc_linux/BUILD.bazel` runs 240 Backend cases, 240 Stack cases, 64 random traces, factory/object/container/boundary/concurrency checks and a soak test through the linked allocator. It does not introduce a per-instance `Backend::TCMalloc`.

The 671 adapter tests also include 114 interface-use cases: ownership adoption,
Shared/Weak lifetime, startup configuration, Stack marks, synchronized handoff and
random ownership transitions with counters on/off.

On this WSL host, the native clock check fails with hardware cycle timing. `UNIMEMORY_VALIDATION_PORTABLE_CLOCK=1` selects Abseil's [supported compile-time clock fallback](https://github.com/abseil/abseil-cpp/blob/20260526.0/absl/base/internal/unscaledcycleclock_config.h); no assertion is changed. WSL's crash collector also blocks intentional death tests; the local validation temporarily bypasses it and restores the original kernel setting. The scripts do not change kernel settings automatically.

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
| Fuzzing/manual TCMalloc targets | Bazel's default test selection applies; do not claim every possible configuration or fuzz corpus ran |
| Android/iOS | Not device-tested in this release |
| GNU `--wrap=mi_heap_new` | The reset-failure fixture requires static UniMemory; the linker cannot wrap calls inside an existing shared library |
| Windows `new` replacement | Allocation-failure injection requires static UniMemory; it cannot replace DLL-internal calls |

No allocation library can convert invalid pointer lifetimes, mismatched frees or concurrent Heap `reset()` into defined behavior without additional mechanisms. UniMemory documents these preconditions and validates legal calls plus specified failure behavior.
