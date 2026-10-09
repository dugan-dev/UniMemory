# Contributing

Keep C++20, existing dependency pins and explicit allocator backends. Discuss new dependencies, backend additions and API changes before implementation. Do not replace global `new`/`delete`, commit credentials, local configuration or build output.

Open an issue with the revision, compiler/platform, backend, smallest reproducer and expected behavior. For a fix, add a regression that fails on the old implementation, explain the root cause, and submit a pull request. Language-contract findings need primary standard evidence in addition to runtime tests.

For code changes, run the full configured CTest suite, independent public-header and installed-consumer checks. Storage, lifetime or backend changes also need Debug/Release, optional backends and relevant sanitizer validation. API/ABI changes must update the [compatibility contract](docs/compatibility.md). For documentation-only changes, run the documentation checker and verify changed commands and links; do not report unrun configurations as passing.

```sh
python tools/check-docs.py
cmake -S . -B build/check -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/check --config Release --parallel 4
ctest --test-dir build/check -C Release --output-on-failure
```

Reuse already installed dependencies without running another manifest against a shared vcpkg prefix; see [backend setup](docs/guides/backends.md). Performance reports need pinned revisions, workload and machine details, native/API scope, statistics settings, raw trials and a stated acceptance budget. The published September measurements are historical, not a performance gate for new code.

Submit changes through a pull request. The build and release-validation checks must pass before merge; report skips, failures and checks that require another platform. Independent review should address the implementation and evidence rather than infer correctness from a test count. Repository rules do not require a sole maintainer to approve their own pull request. Preserve published tags and main-branch history; a repair source snapshot is distinct from the original stable release even when both report library version 0.0.1.

Use the issue and pull-request templates for reproducible reports. See [validation instructions](docs/testing.md) for platform evidence and [SECURITY.md](SECURITY.md) for private vulnerability reports.
