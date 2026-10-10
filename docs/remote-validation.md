# Remote validation

Related source or configuration changes trigger GitHub Actions on `dev`, `main`,
and pull requests. There are no scheduled workflows. Builds and tests for this
development run remotely.

## Platform and compiler matrix

| Native runner | Compilers | Configurations |
| --- | --- | --- |
| Ubuntu 24.04 x64 / ARM64 | GCC 13, GCC 14, Clang 18 | Debug / Release |
| Windows 2022 x64 | MSVC, clang-cl, installed MinGW-w64 GCC | Debug / Release |
| Windows 11 ARM64 | MSVC, clang-cl | Debug / Release |
| macOS 15 Intel / Apple Silicon | AppleClang | Debug / Release |

Actual compiler family, version, SDK where available, and produced binary
architecture are recorded in `toolchain.json`. Compiler major selections are
checked; the build cannot silently use a different target. Existing shared and
optional-backend configurations supplement this matrix. This table does not
claim that every optional allocator supports every compiler/architecture pair.

## Detection and pressure

UBSan is non-recovering, with an isolated overflow control that must fail.
Standard/Stack and three-backend adapter configurations run sanitizer tests.
Native dependency internals are not covered by an adapter-only instrumented
build. Thread adapter jobs additionally use mimalloc's supported TSan build so
its atomics and memory-reuse annotations are visible to the detector. Jemalloc
internals remain uninstrumented; separate mimalloc UBSan/TSan jobs exercise native
instrumentation independently.
Clang TSan supplies strong global new/delete replacements, so the two explicit
global-new failure-injection cases run in ordinary and compatible ASan jobs,
not in the Clang TSan binary. They remain required in the standard build matrix;
this is an instrumentation incompatibility, not a passed skipped test.
Static analysis uses the actual compile database. GCC reports project line and
branch coverage, including translation units with no executed data.

Extended pressure includes 32 waves of four producer threads handing surviving
blocks to newly created consumers, an asymmetric four-producer/one-consumer
case, and a 64 MiB live-set test with sparse survivors across eight cycles.
Data, alignment, lifetime and allocation accounting must remain valid. This is
bounded pressure, not proof of unlimited capacity or multi-day service stability.

libstdc++ 13/14 can destroy partially constructed shared arrays in forward order.
The compatibility test requires the same behavior from a standalone STL call;
all objects and storage must still be reclaimed. Diagnostics record the standard
library limitation rather than attributing it to UniMemory. C++20 requires reverse
order. The UniMemory raw/unique array paths retain their own reverse-order tests.

## Performance and charts

Linux x64, Windows x64 and macOS ARM64 measure matched Standard/mimalloc/jemalloc
native calls against the UniMemory API with statistics disabled and enabled.
Every measurement uses three fresh-process trials. Scaling covers 1/2/4/8/16
threads with same-thread and cross-thread release. Native/API paths receive the
same main-thread and worker warmup before timing. Reports disclose CPU capacity;
oversubscribed thread counts are not additional physical cores.
Each thread executes 4,096 pairs in eight batches of 512, amortizing barrier
scheduling costs. The largest configured scaling live payload is 512 MiB,
leaving runner headroom. Protocol v2 is distinct from the initial 128-batch
experiment; its numbers are not compared against that protocol's baseline.

Individual allocate/free samples provide p50/p95/p99/p99.9 and maximum latency.
Clock overhead is measured separately and remains included in the samples.
Memory probes record RSS and release/collection phases; cached resident memory
is not automatically a leak or fragmentation rate. The existing API sweep and
16 external native application workloads also execute remotely. External process
allocator replacement does not measure the UniMemory adapter.

Correctness failures or incomplete scenarios fail the job. Hosted-runner timing
differences are reported, not used as absolute speed gates. Baseline comparison
requires matching recorded CPU/compiler/protocol; raw samples remain in artifacts.

SVGs are generated without plotting-library dependencies and stored under
`docs/images/performance/<platform-architecture>/`. Summaries and measurements
are stored under `docs/results/current/<platform-architecture>/`; historical
results remain untouched. README displays only the original workload figure;
all generated charts are embedded in the detailed performance report.
Chinese report pages use Chinese SVG variants, including
titles, axes, legends and captions; both languages use identical measurements.
Per-platform latency, statistics cost, retention and workload charts provide
more detail, embedded by dimension and platform directly in the report page.
Source revision and remote-run provenance accompany every report. Publication only
updates generated reports and measurements; it cannot replace README content.

Only trusted successful push runs can publish. A scoped bot branch opens a PR.
Before approving its real PR workflows, the publisher verifies repository, bot
author, branch, source/head revisions and every changed file against the generated
path allowlist. It waits for all five workflows and their actual PR acceptance
checks, rechecks source freshness, then merges the exact checked head. Generated
changes validate existing data without recursively rerunning benchmarks.

GitHub's approval API was verified with the scoped default token in this
[remote probe](https://github.com/dugan-dev/UniMemory/actions/runs/38026443544).
No additional credential is required in this configured repository. An approval
error fails publication and leaves the PR available for review. Dispatched checks
are not substituted for protected-branch checks; see
[GitHub's required-check rules](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/troubleshooting-required-status-checks).
