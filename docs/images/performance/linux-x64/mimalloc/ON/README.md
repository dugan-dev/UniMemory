# Performance · linux-x64/mimalloc/ON

Source: `e355001f2ad885add2447bd085e199206c7a4096` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38097248244)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 7.63 | 9.29 | 1.218 |
| mimalloc | 64 | 7.58 | 9.34 | 1.232 |
| mimalloc | 256 | 7.96 | 9.38 | 1.179 |
| mimalloc | 4096 | 18.44 | 19.66 | 1.066 |
| mimalloc | 65536 | 18.08 | 19.57 | 1.083 |

Profile: `mimalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
