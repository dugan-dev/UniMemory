# Validation

[Documentation](README.md) · **English** · [简体中文](testing.zh-CN.md)

The current header-only library is checked on Windows, Linux and macOS. See the [latest Actions](https://github.com/dugan-dev/UniMemory/actions?query=branch%3Adev) for each tested source revision.

| Area | Checks |
| --- | --- |
| Allocation | Zero size, alignment, overflow, resize and failure rollback |
| Objects and ownership | Construction, destruction, arrays, shared/weak lifetime and cross-thread release |
| Containers | Allocator, PMR, copy, move, nested containers and zero-size PMR requests |
| Heap and Stack | Membership, collect/reset, capacity, marks and rewind |
| Concurrency | Global initialization, allocation handoff and exact request counters |
| Integration | Single entry header, six include orders, multiple translation units, installed profiles and PIC choices |
| Error detection | ASan, non-recovering UBSan, TSan, static analysis and coverage |

Core tests run in Debug/Release with statistics off/on. Optional backends are tested separately. Sanitizers detect executed paths; coverage reports show unexecuted lines and branches.

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

[Build options](getting-started.md#build-options) · [Native suites](upstream-validation.md) · [Performance](performance.md)
