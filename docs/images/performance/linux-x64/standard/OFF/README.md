# Performance · linux-x64/standard/OFF

Source: `e355001f2ad885add2447bd085e199206c7a4096` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38097248244)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 13.38 | 16.12 | 1.205 |
| standard | 64 | 13.43 | 16.10 | 1.199 |
| standard | 256 | 13.37 | 16.10 | 1.204 |
| standard | 4096 | 22.20 | 24.80 | 1.117 |
| standard | 65536 | 24.28 | 25.12 | 1.035 |

Profile: `standard` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
