# Benchmarking

[Index](README.md) · **English** · [简体中文](benchmarking.zh-CN.md) · [Results](performance.md)

Compare allocators on the same machine with the same compiler, runtime and optimization settings. Results from different CPU models cannot rank backends.

## Build

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_BENCHMARKS=ON
cmake --build build/bench --config Release
```

Build each backend/statistics combination separately. Optional SDK setup is described in the [backend guide](guides/backends.md).

## Measurements

| Dimension | Scope |
| --- | --- |
| Allocation | 16–65536 B, matching size and alignment, direct Native/API calls |
| Objects and containers | Construction/destruction, arrays, vector, PMR and ownership |
| Concurrent work | Same-thread and cross-thread allocation/free, 1–16 workers |
| Memory | Live requests, resident memory, release and collection |
| Native applications | 16 programs from the pinned mimalloc-bench collection |

The controlled protocol builds all backends on one runner per platform, warms each path, calibrates meaningful timing intervals, alternates execution order and retains at least ten fresh-process trials. Matched Native/API paths use the same operation count and SDK binary.

Each profile uses three sustained calibration pilots. Oversubscribed scaling is capped at 32 rounds; shorter intervals and calibration limits are disclosed. Scaling includes synchronization costs, so thread counts above runner capacity are pressure scenarios.

Reports include the arithmetic mean, median, minimum, maximum, sample standard deviation, coefficient of variation and bootstrap 95% interval. Every sample is retained. Large variation is reported, not removed or treated as an allocator improvement. RSS phase values are reduced only after subtracting each trial's baseline.

Process-trial data and summaries are stored losslessly in the repository. Individual operation traces and build evidence remain in the linked workflow artifacts for 90 days; traces are checked before publication.

## Relation to mimalloc's benchmarks

[mimalloc's official report](https://github.com/microsoft/mimalloc#performance) uses ten-run averages over a broad application and synthetic suite. [mimalloc-bench](https://github.com/daanx/mimalloc-bench) is the source of the native program subset used here.

UniMemory also measures C++ objects, containers and context handling. Standard uses C++ allocation APIs; it is not the same baseline as the official glibc malloc comparison. Process-wide native allocator replacement measures allocator behavior, not wrapper cost. No allocator is fastest for every workload.

[Data and environments](results/current/README.md) · [Validation](testing.md)

New measurements use `allocation-v3` and `delivery-v2` inventories, including Standard Heap. Older archives retain their original inventory and timing data; they do not measure the current Heap or statistics implementation. Native allocator diagnostics use private build identifiers, separate from the public Memory API. `api_lookup` diagnostics retrieve the same Global singleton for each allocation/free call; `api_cached` reuses a reference retrieved before timing.
