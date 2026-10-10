# Performance · macos-arm64

Source: `6738c7779f3042aaf7e300e55acafdf578946cb0` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38025334938)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 38.23 | 41.62 | 1.089 |
| standard | 64 | 38.10 | 41.72 | 1.095 |
| standard | 256 | 39.14 | 41.91 | 1.071 |
| standard | 4096 | 43.85 | 47.15 | 1.075 |
| standard | 65536 | 272.84 | 276.29 | 1.013 |
| mimalloc | 16 | 4.91 | 6.65 | 1.352 |
| mimalloc | 64 | 5.03 | 6.92 | 1.375 |
| mimalloc | 256 | 6.03 | 7.91 | 1.313 |
| mimalloc | 4096 | 14.62 | 15.95 | 1.091 |
| mimalloc | 65536 | 16.23 | 17.80 | 1.097 |
| jemalloc | 16 | 15.38 | 17.34 | 1.128 |
| jemalloc | 64 | 15.47 | 17.53 | 1.133 |
| jemalloc | 256 | 15.79 | 17.70 | 1.121 |
| jemalloc | 4096 | 20.05 | 22.45 | 1.119 |
| jemalloc | 65536 | 234.73 | 239.98 | 1.022 |

## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
