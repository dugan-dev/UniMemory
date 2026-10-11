# Performance · linux-x64/mimalloc/OFF

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 8.61 | 9.22 | 1.070 |
| mimalloc | 64 | 8.97 | 11.87 | 1.323 |
| mimalloc | 256 | 9.49 | 13.31 | 1.402 |
| mimalloc | 4096 | 23.89 | 24.32 | 1.018 |
| mimalloc | 65536 | 25.21 | 25.78 | 1.023 |

Profile: `mimalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
