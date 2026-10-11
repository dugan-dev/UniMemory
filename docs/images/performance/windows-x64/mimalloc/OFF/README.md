# Performance · windows-x64/mimalloc/OFF

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 7.35 | 7.23 | 0.983 |
| mimalloc | 64 | 6.74 | 7.60 | 1.127 |
| mimalloc | 256 | 7.75 | 8.54 | 1.102 |
| mimalloc | 4096 | 28.74 | 29.35 | 1.021 |
| mimalloc | 65536 | 31.64 | 32.16 | 1.016 |

Profile: `mimalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
