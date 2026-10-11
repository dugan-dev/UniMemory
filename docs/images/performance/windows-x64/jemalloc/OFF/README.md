# Performance · windows-x64/jemalloc/OFF

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 38.32 | 39.69 | 1.036 |
| jemalloc | 64 | 36.98 | 38.14 | 1.031 |
| jemalloc | 256 | 38.92 | 38.18 | 0.981 |
| jemalloc | 4096 | 41.39 | 43.25 | 1.045 |
| jemalloc | 65536 | 325.77 | 332.09 | 1.019 |

Profile: `jemalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
