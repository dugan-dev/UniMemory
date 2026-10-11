# Performance · macos-arm64/jemalloc/OFF

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| jemalloc | 16 | 15.17 | 16.69 | 1.100 |
| jemalloc | 64 | 15.99 | 21.26 | 1.329 |
| jemalloc | 256 | 16.12 | 15.62 | 0.969 |
| jemalloc | 4096 | 22.29 | 20.83 | 0.935 |
| jemalloc | 65536 | 248.32 | 262.33 | 1.056 |

Profile: `jemalloc` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


Baseline comparison withheld: recorded hardware, compiler or measurement protocol differs.
