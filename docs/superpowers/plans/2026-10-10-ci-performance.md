# Remote validation and performance publication implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Complete the five agreed validation and visualization requirements on `dev`, with remote evidence for every supported configuration.

**Architecture:** Separate platform/compiler validation, instrumented validation,
performance measurement, and generated-report publication. Relevant source or
configuration changes trigger all applicable jobs; generated-only changes do
not recursively trigger benchmarks. Protected main is updated only through PRs.

**Tech Stack:** C++20, CMake/CTest, existing allocator versions, GitHub Actions,
existing compiler tools, Python standard library, generated SVG.

**Spec:** `docs/superpowers/specs/2026-10-10-ci-performance-design.md`

## Global constraints

- Develop on `dev`; do not merge to main before remote acceptance.
- Run builds and tests remotely only; no scheduled triggers.
- Preserve public interfaces and allocator dependency versions.
- No new allocator or plotting-library dependencies.
- Keep README prose/layout; update chart references and their short units/direction caption.
- Compare native allocation, UniMemory API, and enabled statistics separately.
- Preserve historical data and original backend replacement boundaries.

## Review focus

- UB that reports an error but exits successfully must fail CI.
- Wrong compiler selection or missing ARM toolchain must not count as coverage.
- Missing/failed benchmark scenarios must prevent publication of a complete report.
- Native backend replacement must not contaminate the Standard baseline.
- Bot-generated commits must not loop or bypass protected-branch checks.

## Task 1: Native architecture and compiler validation

**Files:** `.github/workflows/ci.yml`, reusable workflow/toolchain helpers under
`.github/workflows/` and `tools/ci/`, `CMakePresets.json`, validation documentation.
**Interface:** Compiler selection produces a recorded compiler/SDK manifest;
all configurations use source build, CTest, install, and installed consumer.

- [ ] Create the explicit supported platform/compiler matrix from the spec.
- [ ] Verify actual runner tools and supported dependency combinations; pin mature
  compiler choices and record versions. Reject silent compiler substitution.
- [ ] Validate x64/ARM64 on Windows/Linux and both macOS architectures, including
  GCC, Clang, AppleClang, MSVC, clang-cl and MinGW-w64 GCC where applicable.
- [ ] Validate Debug/Release and representative shared-library consumers.
- [ ] Push `dev`, inspect every remote job, and repair actual failures.

## Task 2: Strict detection, static analysis and coverage

**Files:** `CMakeLists.txt`, `.github/workflows/release-validation.yml`, new isolated
sanitizer probe under `tests/`, analysis/coverage helpers under `tools/ci/`.
**Interface:** Detection jobs emit logs and explicit scope; coverage emits project
line/branch summaries and source reports excluding dependency code.

- [ ] Add the isolated UB probe and a remote test of its required nonzero exit.
- [ ] Configure UBSan non-recovery and empty-suite rejection consistently.
- [ ] Extend supported optional-backend adapter detection; distinguish instrumented
  project code from instrumented allocator internals.
- [ ] Add static analysis with an explicit check set and diagnostic failure policy.
- [ ] Collect line/branch coverage and identify uncovered contract paths.
- [ ] Add focused tests for confirmed gaps; verify remote red/green evidence.

## Task 3: Stronger correctness and stress workloads

**Files:** Existing `tests/production_tests.cpp`, `tests/interface_usage_tests.cpp`,
focused new workload sources if necessary, CMake registrations and CI helpers.
**Interface:** Workloads accept recorded seeds, thread counts and bounded scale;
failure means content/lifetime/accounting/contract violation or unexpected error.

- [ ] Add bounded asymmetric handoff and repeated thread-start/exit scenarios.
- [ ] Expand live sets, mixed lifetimes, large allocations and controlled failures.
- [ ] Keep exclusive-reset preconditions and declared memory budgets.
- [ ] Execute longer pressure on relevant changes with explicit job timeouts.
- [ ] Retain failed seeds and full diagnostics; inspect remote results.

## Task 4: Comparable performance measurements

**Files:** `benchmarks/`, `tools/run-benchmarks.py`, `tools/run-upstream-benchmarks.py`,
focused measurement/validation helpers and new performance workflow.
**Interface:** Raw CSV plus JSON manifest contain source revision, compiler,
machine capacity, parameters, sample count and measurement provenance.

- [ ] Validate matching Native/API sizes, alignment and touching; verify the
  Standard provider remains unmodified.
- [ ] Extend throughput scaling to 1/2/4/8/16 threads, same-thread/cross-thread
  work, size distributions and lifetime mixes.
- [ ] Measure real operation-latency percentiles, timing overhead, RSS, retention
  and memory phases; retain raw samples and independent process trials.
- [ ] Run the existing 16 external native workloads on relevant changes and keep
  their results separate from UniMemory wrapper measurements.
- [ ] Compare comparable revisions and report noise/regression signals without
  inventing an absolute speed gate on variable hosted hardware.
- [ ] Exercise report validation with missing scenarios, failed runs and differing
  environments in remote tool tests.

## Task 5: Charts and protected publication

**Files:** Python SVG renderer/report validator, publication workflow,
`docs/images/performance/`, `docs/results/current/`, performance documentation,
README image references only.
**Interface:** Validated CSV/JSON produce deterministic SVG assets; publish a
complete revision-specific set through a bot PR with exact artifact provenance.

- [ ] Generate consistent column and line charts for throughput, memory cost,
  latency and workloads, with readable units and Native/API series names.
- [ ] Test invalid/missing input rejection and XML-valid chart output remotely.
- [ ] Gate publication on complete successful trusted-branch measurements.
- [ ] Limit write permissions to publication, allowlist generated paths, verify
  artifact revision, and approve/wait for real required PR checks for bot PRs.
- [ ] Update README fixed image links and preserve historical results.
- [ ] Prevent generated-only commits from triggering benchmark/publication loops.

## Task 6: Whole-branch review and remote acceptance

- [ ] Review all changes against each spec requirement, including unsupported
  combinations, skip accounting, branch protection and publication permissions.
- [ ] Obtain an independent whole-branch review and fix important findings.
- [ ] Verify remote platform/compiler, detection, analysis, coverage, stress and
  performance jobs; inspect chart assets and the generated-results PR.
- [ ] Report actual passed, failed, skipped or blocked configurations explicitly.
- [ ] Keep development on `dev`; offer the completed, remotely verified PR for
  main integration without claiming unsupported configurations are complete.
