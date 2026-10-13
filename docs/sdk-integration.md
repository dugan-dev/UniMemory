# SDK integration

[Documentation](README.md) · [Performance](performance.md)

The [release workflow](../.github/workflows/release-validation.yml) builds pinned mimalloc 3.4.3 and jemalloc 5.3.1 SDKs, then verifies UniMemory and its installed consumers with shared and static SDK linkage.

| Backend | Integration checks |
| --- | --- |
| Standard | No allocator dependency; UniMemory tests and installed consumers |
| mimalloc | Explicit API, shared/static linkage and installed consumers |
| jemalloc | Explicit `je_` API, shared linkage and installed consumers |

Third-party source code and versions are unchanged. Their native unit and stress suites are not run here. Sanitizer compatibility builds support testing UniMemory's allocation lifetimes and concurrent use.

[Testing](testing.md) · [Actions](https://github.com/dugan-dev/UniMemory/actions?query=branch%3Adev)
