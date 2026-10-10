# Performance · linux-x64

Source: `6738c7779f3042aaf7e300e55acafdf578946cb0` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38025334938)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 16.84 | 21.46 | 1.274 |
| standard | 64 | 16.92 | 21.54 | 1.273 |
| standard | 256 | 16.85 | 21.48 | 1.275 |
| standard | 4096 | 29.67 | 33.39 | 1.126 |
| standard | 65536 | 32.38 | 36.17 | 1.117 |
| mimalloc | 16 | 12.98 | 10.89 | 0.839 |
| mimalloc | 64 | 9.03 | 11.17 | 1.237 |
| mimalloc | 256 | 9.29 | 11.88 | 1.279 |
| mimalloc | 4096 | 23.48 | 26.87 | 1.144 |
| mimalloc | 65536 | 25.81 | 28.53 | 1.105 |
| jemalloc | 16 | 19.70 | 24.13 | 1.225 |
| jemalloc | 64 | 19.08 | 24.17 | 1.267 |
| jemalloc | 256 | 19.03 | 24.36 | 1.280 |
| jemalloc | 4096 | 22.19 | 27.50 | 1.240 |
| jemalloc | 65536 | 372.37 | 374.99 | 1.007 |

## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
