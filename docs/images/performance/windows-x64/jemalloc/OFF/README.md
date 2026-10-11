# Performance · windows-x64/jemalloc/OFF

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 32.87 | 33.53 | 1.020 |
| jemalloc | 64 | 33.23 | 32.99 | 0.993 |
| jemalloc | 256 | 33.37 | 33.65 | 1.008 |
| jemalloc | 4096 | 35.90 | 36.52 | 1.017 |
| jemalloc | 65536 | 289.72 | 289.63 | 1.000 |

Profile: `jemalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
