# Performance · macos-arm64/standard/OFF

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 16.64 | 16.99 | 1.021 |
| standard | 64 | 18.27 | 20.01 | 1.095 |
| standard | 256 | 18.82 | 19.06 | 1.013 |
| standard | 4096 | 22.53 | 22.84 | 1.014 |
| standard | 65536 | 24.59 | 25.01 | 1.017 |

Profile: `standard` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
