# Performance · macos-arm64/jemalloc/ON

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 17.56 | 18.95 | 1.079 |
| jemalloc | 64 | 16.84 | 21.71 | 1.289 |
| jemalloc | 256 | 17.45 | 18.59 | 1.065 |
| jemalloc | 4096 | 22.03 | 24.55 | 1.114 |
| jemalloc | 65536 | 266.27 | 242.81 | 0.912 |

Profile: `jemalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
