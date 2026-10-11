# Performance · windows-x64/jemalloc/ON

Source: `e355001f2ad885add2447bd085e199206c7a4096` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38097248244)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 38.67 | 41.85 | 1.082 |
| jemalloc | 64 | 39.28 | 43.78 | 1.114 |
| jemalloc | 256 | 39.96 | 41.70 | 1.044 |
| jemalloc | 4096 | 42.71 | 44.81 | 1.049 |
| jemalloc | 65536 | 324.95 | 328.84 | 1.012 |

Profile: `jemalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
