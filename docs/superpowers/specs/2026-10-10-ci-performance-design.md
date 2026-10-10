# UniMemory CI and performance design

## Outcome and constraints

Expand remote validation across mainstream platforms and compilers, strengthen
failure detection, and publish reproducible performance charts automatically.
All builds and test execution for this work run in GitHub Actions, not locally.
Develop on the user-requested `dev` branch. Relevant changes trigger validation
and performance measurement automatically. Do not add cron/scheduled triggers.
Keep C++20, the public API, and existing allocator dependency versions unchanged.
Do not add allocation backends or benchmark-library dependencies.
Keep the README layout and prose; only replace performance image references when
the generated assets are ready. Preserve existing historical results.

## Platform and compiler coverage

Use explicit runner labels and record actual compiler versions in each artifact.
Validate installed consumers as well as source builds.

| Platform | Architecture | Compiler family |
| --- | --- | --- |
| Ubuntu 24.04 | x64 / ARM64 | GCC / Clang |
| Windows 2022 | x64 | MSVC / clang-cl / MinGW-w64 GCC |
| Windows 11 ARM | ARM64 | MSVC / clang-cl |
| macOS 15 Intel | x64 | AppleClang |
| macOS 15 | ARM64 | AppleClang |

Prefer GCC 13/14 and Clang 18 where available, Visual Studio 2022's supported
toolchain on Windows x64, and stable toolchains available on native ARM runners.
Implementation must verify runner tool availability rather than silently fall
back to a different compiler. Record AppleClang, clang-cl, MinGW, SDK, and native
Windows ARM toolchain versions. No unsupported compiler/platform cross-product.
Each architecture receives Debug and Release checks. Representative compilers
also validate shared libraries. Existing optional-backend checks remain required;
additional combinations are enabled only where the pinned dependencies support
the target. Missing support is reported explicitly, never counted as a pass.

## Failure detection

UBSan must stop on the first detected error. Include a deliberately invalid
isolated probe to demonstrate that the configured CI invocation rejects UB;
the probe is not part of the normal library test workload.
CTest must reject empty test suites. Final acceptance requires every required
job to succeed; unexpected skips cannot satisfy acceptance.

Retain Standard/Stack ASan+UBSan and TSan jobs. Add optional-backend adapter
validation using supported instrumentation configurations, keeping checks of
UniMemory code distinct from checks of allocator internals. Native mimalloc
instrumentation follows the pinned upstream build's supported flags. Do not
claim that uninstrumented dependency internals have sanitizer coverage.

Add static analysis of project sources with the compilation database and an
explicit, reviewable check set. Capture line and branch coverage for project
sources; exclude third-party code and generated files. Initially report measured
coverage and uncovered branches rather than inventing a percentage threshold.
Add focused tests for gaps uncovered by review or remote measurements.

## Correctness and stress workloads

Reuse current data-content, alignment, lifetime, accounting, exception-safety,
and cross-thread checks. Extend parameterized workloads to thread churn,
asymmetric allocation/free handoff, large live sets, and controlled failures.
Use bounded, deterministic workloads on PRs. Put longer randomized and memory
pressure workloads in change-triggered/manual validation with seeds, sizes, thread
counts and duration captured in the report. All pressure tests use a declared
memory budget and leave runner headroom; they must not depend on host OOM kills.
Do not test concurrent reset where the public contract requires exclusive access.

## Performance measurement

Use existing C++ benchmarks and Python standard-library tools. Compare:

- Standard native allocation versus UniMemory Standard.
- Native mimalloc versus UniMemory mimalloc.
- Native jemalloc versus UniMemory jemalloc.
- Statistics disabled versus enabled, shown as distinct series.

Keep native and wrapper requests, alignment, content touching, and workloads
matched. Keep process-wide replacement benchmarks separate from explicit API
benchmarks. Reuse the already-approved rpmalloc native test source as an optional
external comparison; do not present it as a UniMemory backend. Additional
allocators require separate dependency approval.

Measure size distributions, same-thread and cross-thread lifetimes, throughput
at 1/2/4/8/16 threads, single-operation latency distributions, RSS/peak RSS,
retention after release/collect, and the existing object/container workloads.
Report actual CPU capacity and distinguish oversubscribed workloads. Measure
timing overhead; do not label percentiles of batch averages as per-operation
p95/p99/p99.9. RSS retention is not itself proof of a leak or fragmentation.
Reuse the existing 16 external application workloads for periodic native
comparison on relevant changes. Run fresh-process repetitions and preserve all raw samples.

Correctness failures fail the benchmark job. Performance comparisons on hosted
runners initially produce reports and regression signals, not an absolute speed
gate. A blocking performance threshold requires a stable, comparable baseline
and repeated evidence. Never merge numbers from different hardware as if they
were the same environment.

## Chart publication

Generate deterministic SVG charts from measured CSV/JSON, without new plotting
dependencies. Use consistent series colors, typography, units and legends.
Provide column charts for workload comparisons and line charts for scaling and
memory trends. Avoid perspective effects that distort quantitative comparisons.
Show native/API labels explicitly and include environment and revision metadata.

Publish current results under `docs/results/current/<platform-architecture>/`
and charts under `docs/images/performance/`. README references two summary charts:
throughput scaling and memory cost. Detailed charts cover latency, workload
comparisons, and memory phases in performance documentation.

Change-triggered/manual benchmark runs upload immutable artifacts first. Only a complete
validated report may update the tracked current-results paths. Publication uses
a bot PR compatible with branch protection, scoped to generated results and
images, with automatic merge after required validation and temporary branch
deletion. Do not push generated artifacts directly around branch protection.
Avoid recursive benchmark triggers for generated-result-only commits. Keep write
permissions limited to the publication job and never available to untrusted PRs.

## Workflow separation and acceptance

1. PR: platform/compiler build, correctness, consumers, static analysis, coverage,
   and bounded sanitizer checks.
2. Change-triggered/manual extended validation: long stress, supported native allocator
   detection, external application workloads, and full benchmarks.
3. Publication: validate report completeness, generate charts, open/update a bot
   PR, and retain provenance linking every chart to its run and source revision.

Acceptance requires remote evidence for the advertised platform/compiler matrix,
strict sanitizer failure behavior, static-analysis results, coverage artifacts,
successful benchmark runs, and a generated-results PR. A chart must never contain
fabricated data, an unsupported backend, or an omitted failed scenario presented
as a successful complete comparison. Historical reports remain unchanged.
