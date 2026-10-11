# Performance · macos-arm64/mimalloc/ON

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 5.01 | 6.77 | 1.350 |
| mimalloc | 64 | 5.26 | 7.59 | 1.443 |
| mimalloc | 256 | 6.46 | 7.45 | 1.153 |
| mimalloc | 4096 | 14.96 | 16.85 | 1.126 |
| mimalloc | 65536 | 16.95 | 17.96 | 1.059 |

Profile: `mimalloc` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
