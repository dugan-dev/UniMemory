# Contributing

Keep C++20, existing dependency pins and explicit allocator backends. Do not replace global `new`/`delete`, commit credentials, local configuration or build output.

Open an issue with the revision, compiler/platform, backend, smallest reproducer and expected behavior. For a fix, add a regression that fails on the old implementation, explain the root cause, and submit a pull request. Language-contract findings need primary standard evidence in addition to runtime tests.

Run the documentation check, full configured CTest suite, independent public-header and installed-consumer checks. Changes to storage, lifetime or backends also need Debug/Release, optional backends and sanitizer validation. API/ABI changes must update the compatibility contract.

```sh
python tools/check-docs.py
cmake -S . -B build/check -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/check --parallel 4
ctest --test-dir build/check --output-on-failure
```

Use `--config Release` and `-C Release` with multi-configuration generators. See [validation instructions](docs/README.md) for backend and platform specifics. Report security issues through the process in [SECURITY.md](SECURITY.md).
