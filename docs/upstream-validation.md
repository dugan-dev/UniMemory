# Native allocator validation

[Documentation](README.md) · [Performance](performance.md)

The [release workflow](../.github/workflows/release-validation.yml) runs pinned native correctness and pressure suites alongside UniMemory tests. SDK versions remain mimalloc 3.4.3 and jemalloc 5.3.1.

| Allocator | Validation |
| --- | --- |
| mimalloc | Native unit tests, sanitizer builds, stress and static/shared packaging |
| jemalloc | Native unit/integration/stress tests, explicit-prefixed API and packaging |
| rpmalloc | Pinned upstream correctness and threaded stress suite |

These checks exercise native allocators separately; they do not replace tests of UniMemory ownership, failure handling and counters. Current results are attached to each [Actions run](https://github.com/dugan-dev/UniMemory/actions?query=branch%3Adev).
