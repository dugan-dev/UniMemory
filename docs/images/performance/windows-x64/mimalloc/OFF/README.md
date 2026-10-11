# Performance · windows-x64/mimalloc/OFF

Source: `e355001f2ad885add2447bd085e199206c7a4096` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38097248244)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 6.42 | 7.24 | 1.129 |
| mimalloc | 64 | 6.75 | 7.57 | 1.121 |
| mimalloc | 256 | 7.68 | 8.55 | 1.113 |
| mimalloc | 4096 | 28.75 | 29.35 | 1.021 |
| mimalloc | 65536 | 31.52 | 32.15 | 1.020 |

Profile: `mimalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
