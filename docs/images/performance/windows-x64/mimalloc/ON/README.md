# Performance · windows-x64/mimalloc/ON

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 6.78 | 30.55 | 4.506 |
| mimalloc | 64 | 7.19 | 30.79 | 4.281 |
| mimalloc | 256 | 8.57 | 31.75 | 3.706 |
| mimalloc | 4096 | 28.32 | 50.90 | 1.797 |
| mimalloc | 65536 | 29.41 | 53.24 | 1.810 |

Profile: `mimalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
