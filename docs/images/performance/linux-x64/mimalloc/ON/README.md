# Performance · linux-x64/mimalloc/ON

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 8.64 | 10.38 | 1.202 |
| mimalloc | 64 | 8.91 | 10.32 | 1.158 |
| mimalloc | 256 | 9.81 | 10.92 | 1.113 |
| mimalloc | 4096 | 24.01 | 24.57 | 1.023 |
| mimalloc | 65536 | 25.71 | 26.55 | 1.032 |

Profile: `mimalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
