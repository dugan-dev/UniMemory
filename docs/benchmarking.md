# Benchmarking

[Index](README.md) · **English** · [简体中文](benchmarking.zh-CN.md) · [Results](performance.md)

Measure your workload. [mimalloc-bench](https://github.com/daanx/mimalloc-bench) provides application and synthetic workloads; it is a benchmark collection, not an industry certification.

## 1 · Build

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BUILD_BENCHMARKS=ON
cmake --build build/bench --config Release
```

Optional backends need their [build options and dependencies](guides/backends.md). Standard requires no allocator dependency.

## 2 · Measure

Linux / macOS:

```sh
python3 tools/run-benchmarks.py build/bench/UniMemoryDeliveryBenchmark build/results --backends standard
python3 tools/run-sweep.py build/bench/UniMemoryBenchmark build/results
```

Windows: use `python` and executables under `build/bench/Release/` with the `.exe` suffix. For three enabled backends, pass `--backends standard mimalloc jemalloc`; the driver defaults to all three.

| Probe | Workload | Output |
| --- | --- | --- |
| Native / API / Basic | 16–65536 B, alignment 16, allocation/free pairs | latency.csv |
| Memory usage | 16384 × 4096 B, fully touched; live/free/collect | footprint.csv |
| Mixed sizes | 16384 Blocks, sparse survivors, eight refill/free cycles | pressure.csv |
| Heap controls | owns, collect, allocate 64 B + reset | heap.csv |
| API sweep | Bytes, Objects, Arrays, Containers, ownership, handoff, diagnostics, Stack | full.csv |
| Environment | Native versions, type sizes, source/executable SHA-256 | environment.json, sweep-environment.json |

The sweep runs Global Disabled and Global Basic in separate processes because Global configuration is fixed before its first lookup. Heap Disabled/Basic and Stack run only once per trial.

## 3 · Method

| Probe | Repetition |
| --- | --- |
| Raw latency | Warmup; nine × 200000 operations per process; three fresh process trials |
| Full sweep | Warmup; seven repetitions, shuffled Backend order; three fresh process trials |
| Memory | Three fresh processes; page writes and live-content checks |

An operation is the complete named workload. A vector case grows and destroys 32 elements; resize allocates, resizes and frees. Handoff allocates on one thread and frees on 2/4/8 workers; worker creation is outside timing, completion is inside. Heap creation is outside ordinary allocation timing; the reset probe includes replacement Heap setup.

The mixed-size trace uses seed `0xC0FFEE`, sizes 17/33/65/129/257/513/1025/2049/4097/8193 B, alignment 16 and 1024 persistent Blocks. It checks content, alignment, counts and native usable capacity. Diagnostics are outside timed allocation loops. RSS includes allocator caches, metadata and process state; retention is not a fragmentation rate.

The native-statistics workload measures the diagnostic call, including an empty
result for an unsupported Memory. Consult instance capabilities; an empty-result
call is not equivalent to collecting native metrics.

## 4 · Keep backends separate

mimalloc: `MI_OVERRIDE=OFF`, Windows `MI_WIN_REDIRECT=OFF`. Windows children also set and verify `MIMALLOC_DISABLE_REDIRECT=1`.

Unix jemalloc: `--with-jemalloc-prefix=je_ --disable-cxx`. A prefix alone can still replace global C++ new/delete. CMake inspects symbols; the benchmark verifies the Unix global-new provider. Native Standard uses throwing aligned new; the API uses nothrow aligned new and translates failure. Successful size/alignment match; failure behavior differs.

## 5 · Reproduce tables and figures

```sh
python3 tools/summarize-benchmarks.py docs/results/0.0.1 --output build/comparisons.json
```

The summary validates the published two-platform, three-backend matrix. All Python scripts use the standard library.

Optional figure export uses Windows .NET Framework's chart library:

```powershell
powershell -NoProfile -File tools/render-benchmarks.ps1 -InputJson build/comparisons.json -OutputDirectory build/charts
```

CPU affinity is unpinned. Small differences may be noise. These short tests do not establish p95/p99, NUMA scaling, long-running fragmentation or mobile performance.

[Raw measurements](results/0.0.1/README.md) · [Native suites and applications](upstream-validation.md)
