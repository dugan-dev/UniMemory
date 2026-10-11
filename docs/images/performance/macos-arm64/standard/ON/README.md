# Performance · macos-arm64/standard/ON

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 17.07 | 18.89 | 1.107 |
| standard | 64 | 20.82 | 21.40 | 1.028 |
| standard | 256 | 20.05 | 20.79 | 1.037 |
| standard | 4096 | 23.26 | 25.03 | 1.076 |
| standard | 65536 | 25.68 | 26.88 | 1.047 |

Profile: `standard` / statistics `ON` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
