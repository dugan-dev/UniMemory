# Performance · windows-x64/mimalloc/ON

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 7.34 | 9.83 | 1.339 |
| mimalloc | 64 | 6.94 | 10.04 | 1.447 |
| mimalloc | 256 | 7.77 | 10.94 | 1.407 |
| mimalloc | 4096 | 28.74 | 30.64 | 1.066 |
| mimalloc | 65536 | 31.52 | 33.42 | 1.060 |

Profile: `mimalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
