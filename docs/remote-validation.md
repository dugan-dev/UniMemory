# Remote validation

Source and configuration changes trigger GitHub Actions on `dev`, `main` and pull requests. There are no scheduled runs.

## Builds and integration

| Platform | Compilers | Builds |
| --- | --- | --- |
| Windows x64 / ARM64 | MSVC, clang-cl; MinGW-w64 GCC on x64 | Debug / Release |
| Linux x64 / ARM64 | GCC 13/14, Clang 18 | Debug / Release |
| macOS Intel / Apple Silicon | AppleClang | Debug / Release |

Both statistics settings are tested. Separate SDK profiles verify Standard, mimalloc and jemalloc, including installed consumers. Each artifact records the actual compiler, architecture and dependency versions. Optional SDK support varies by platform.

## Correctness

Allocation, alignment, object lifetime, containers, heaps, stacks and concurrent handoffs must pass. Installed consumers verify integration and conflicting build profiles. ASan, non-recovering UBSan, TSan, static analysis and coverage add independent checks. Third-party SDKs are built for integration, without running their own test suites.

Sanitizer instrumentation does not imply full coverage of third-party internals. Compiler-specific limitations and skipped incompatible failure-injection cases are recorded explicitly. See [testing](testing.md) and [SDK integration](sdk-integration.md).

## Performance

Each platform builds all six backend/statistics profiles on one runner. At least ten fresh-process trials use a shuffled order and calibrated operation counts. Native and UniMemory paths share the same SDK binary, compiler, runtime and optimization options.

Reports retain arithmetic mean, minimum, maximum, median, sample deviation, CV and a bootstrap confidence interval. Every process-trial observation is retained in the repository; individual operation traces remain in workflow artifacts for 90 days and are validated before publication. No outliers are removed. Memory deltas are calculated per process before aggregation. The 16-program native application subset runs on the same Linux host.

Correctness failures, incomplete scenarios and inconsistent provenance fail validation. Statistics-disabled Native/API cases require both paired-median and arithmetic-mean performance ratios of at least 90%. Hosted-runner variability and every failed case are retained. See [benchmarking](benchmarking.md).

## Publication

Successful trusted push runs update the current data, 36 bilingual SVGs, original PNG figures and marked performance tables. User prose and scenario labels are preserved.

The publisher opens a scoped generated-results PR, checks its author, repository, source/head and file allowlist, then waits for the complete CI and performance acceptance. It merges only the verified head if the measured source is still current. Generated-only updates validate retained data without rerunning benchmarks.

[Current results](results/current/README.md) · [Actions](https://github.com/dugan-dev/UniMemory/actions?query=branch%3Adev)
