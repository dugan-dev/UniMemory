# Performance · macos-arm64/jemalloc/OFF

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 20.88 | 17.61 | 0.843 |
| jemalloc | 64 | 18.44 | 24.98 | 1.355 |
| jemalloc | 256 | 19.65 | 17.40 | 0.886 |
| jemalloc | 4096 | 23.13 | 24.19 | 1.046 |
| jemalloc | 65536 | 282.19 | 291.29 | 1.032 |

Profile: `jemalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
