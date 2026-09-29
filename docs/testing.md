# Tests

[Documentation](README.md) · **English** · [简体中文](testing.zh-CN.md)

## 0.0.1 verification

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
