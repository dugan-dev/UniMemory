# Tests

[Documentation](README.md) · **English** · [简体中文](testing.zh-CN.md)

## Remote validation · 2026-10-10

The expanded validation targets source `ca4919f`. The [native matrix](remote-validation.md#platform-and-compiler-matrix)
checks 26 Debug/Release compiler configurations, including all six requested
platform/architecture combinations, and verifies installed consumers.

| Evidence | Scope |
| --- | --- |
| [Build](https://github.com/dugan-dev/UniMemory/actions/runs/38027100987) | Standard, optional backends, shared libraries, examples, installation and documentation |
| [Portability](https://github.com/dugan-dev/UniMemory/actions/runs/38027100962) | Actual compiler/version and binary architecture; Debug/Release |
| [Diagnostics](https://github.com/dugan-dev/UniMemory/actions/runs/38027100952) | Strict UBSan control, adapter/native sanitizers, static analysis and coverage |
| [Release validation](https://github.com/dugan-dev/UniMemory/actions/runs/38027100940) | Existing native suites, sanitizer and pressure validation |
| [Performance](https://github.com/dugan-dev/UniMemory/actions/runs/38027100953) | Complete three-platform measurements, report regression tests and generated-results publication |

The GCC coverage configuration executes 1,673 registered tests. Its instrumented
project code reports **604/635 lines (95.12%)** and **337/475 branches (70.95%)**;
unexecuted translation units are included. This is configuration-specific evidence,
not complete template instantiation or exhaustive path coverage. Source reports
identify remaining allocation-failure and capability branches.

Clang TSan runs 1,671 cases; its two incompatible global-new failure-injection
cases remain required in ordinary and compatible ASan builds. Jemalloc dependency
internals remain uninstrumented. [Detection scope and stress scale](remote-validation.md#detection-and-pressure)

The report regressions first failed remotely on `568cc2b` for generated EOF format
and indistinguishable statistics columns. On `6738c77`, all 17 tool tests pass,
including first/different baselines, incomplete scenarios and SVG validity.
The final tool suite passes all 23 cases, including publication identity, scope
and complete file-pagination guards. [Generated-results PR #5](https://github.com/dugan-dev/UniMemory/pull/5)
passed all five genuine PR acceptance checks before automatic merge. All 18 SVGs
identify measured source `ca4919f`; no dispatched check substituted for PR validation.

## Review verification: 2026-10-08 to 2026-10-09

| Configuration | Passed / registered | Installed consumers |
| --- | --- | --- |
| Windows / MSVC, static, three backends, Release | **1666/1666** | **7/7** |
| Windows / MSVC, static, three backends, Debug | **1666/1666** | **7/7** |
| Windows / MSVC, shared, three backends, Release | **1664/1664** | **7/7** |
| Linux / GCC, static, three backends, Release | **1667/1667** | **7/7** |
| Linux / GCC, shared, three backends, Release | **1666/1666** | **7/7** |
| Linux / Clang, Standard, ASan + UBSan + leak detection | **715/715** | **7/7** |

All rows have zero failures. Totals include parameterized cases, package discovery,
header order and stress tests; repeated configurations are not additional unique
features. Executable-level allocation interception differs in shared builds.
GitHub verified repair commit `65c4a46` on 2026-10-09: all 13
[cross-platform build jobs](https://github.com/dugan-dev/UniMemory/actions/runs/37871859097)
and all five [release validation jobs](https://github.com/dugan-dev/UniMemory/actions/runs/37871859066)
passed, including macOS, ASan and ThreadSanitizer. The earlier numerical results
below remain historical. [Review findings and contracts](review-2026-10-08.md)

The subsequent documentation revision `08d97f0` also passed all 13 [build jobs](https://github.com/dugan-dev/UniMemory/actions/runs/37873043811). Each linked run verifies its recorded revision; it is not a guarantee for every later main-branch commit or the earlier `v0.0.1` release.

## Historical 0.0.1 verification: 2026-09-28

| Build | Result |
| --- | --- |
| Windows x64 / MSVC, three backends | Release **1626/1626**, Debug **1626/1626** |
| Linux x64 / GCC / WSL, three backends | **1627/1627** |
| macOS / Apple Clang, three backends | **1626/1626** in GitHub CI; installed package builds and runs |
| Linux Standard / Stack, ASan + UBSan | **681/681**, no sanitizer findings |
| Linux Standard / Stack, ThreadSanitizer | **678/678**, no race findings; examples disabled |
| Installed package, Windows and Linux | Package lookup, version and application build pass |
| Independent GitHub clone | README build, Standard **681/681**; static/shared installed packages **2/2** each |
| Documentation examples | 18 README examples + 2 PMR programs; Linux Standard and three-backend builds, **40/40** runs |

Backend versions: mimalloc 3.4.3 and jemalloc 5.3.1. Results cover the unified Memory interface on the listed configurations.

## Coverage

| Topic | Verified behavior |
| --- | --- |
| Allocation and resize | Zero size, alignment, overflow, preserved content, zeroed growth, failure rollback |
| Objects and smart pointers | Construction, destruction, arrays, adoption, shared and weak ownership |
| Containers | Allocator, PMR, copy, move and swap |
| Heap and Stack | Membership, bulk release, rewind and capacity failure |
| Threads and statistics | Concurrent allocation/free, instance lookup and request counters |
| Stress | Repeated allocation, random operations and ownership handoff |

Callers must avoid mismatched frees, invalid pointers, use after reset and unsynchronized object writes. See the [compatibility guide](compatibility.md#standard-library-behavior) for the shared-array standard-library limitation.

## Run

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

Standard is enabled by default; optional backends need their build options and dependencies. [Configuration](guides/backends.md)

## Automated verification

[Cross-platform builds](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml) check platforms, backends and installed packages. [Release validation](https://github.com/dugan-dev/UniMemory/actions/workflows/release-validation.yml) runs sanitizers, stress tests and upstream suites.

Android/iOS are not device-verified. Results apply only to the listed configurations and workloads.

[Detailed validation](upstream-validation.md) · [Performance](performance.md)
