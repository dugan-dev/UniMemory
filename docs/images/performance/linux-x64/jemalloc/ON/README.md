# Performance · linux-x64/jemalloc/ON

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 19.18 | 21.81 | 1.137 |
| jemalloc | 64 | 19.25 | 22.44 | 1.166 |
| jemalloc | 256 | 19.18 | 21.99 | 1.147 |
| jemalloc | 4096 | 22.32 | 24.94 | 1.118 |
| jemalloc | 65536 | 364.37 | 365.37 | 1.003 |

Profile: `jemalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
