# Performance · windows-x64/jemalloc/ON

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 38.79 | 43.79 | 1.129 |
| jemalloc | 64 | 40.05 | 44.40 | 1.109 |
| jemalloc | 256 | 37.39 | 41.79 | 1.118 |
| jemalloc | 4096 | 43.76 | 46.74 | 1.068 |
| jemalloc | 65536 | 329.10 | 333.42 | 1.013 |

Profile: `jemalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
