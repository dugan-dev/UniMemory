# Performance · macos-arm64/standard/ON

Source: `e355001f2ad885add2447bd085e199206c7a4096` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38097248244)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 18.09 | 23.15 | 1.280 |
| standard | 64 | 19.69 | 21.68 | 1.101 |
| standard | 256 | 19.12 | 20.67 | 1.081 |
| standard | 4096 | 22.41 | 24.27 | 1.083 |
| standard | 65536 | 24.52 | 27.05 | 1.103 |

Profile: `standard` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
