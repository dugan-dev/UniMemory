# Performance · macos-arm64/mimalloc/OFF

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| mimalloc | 16 | 5.81 | 6.15 | 1.059 |
| mimalloc | 64 | 5.88 | 6.09 | 1.036 |
| mimalloc | 256 | 6.97 | 7.45 | 1.069 |
| mimalloc | 4096 | 16.51 | 18.40 | 1.115 |
| mimalloc | 65536 | 17.16 | 17.93 | 1.045 |

Profile: `mimalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
