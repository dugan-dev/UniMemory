# Performance · windows-x64

Source: `6738c7779f3042aaf7e300e55acafdf578946cb0` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38025334938)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 45.15 | 49.26 | 1.091 |
| standard | 64 | 46.24 | 51.18 | 1.107 |
| standard | 256 | 47.19 | 50.85 | 1.078 |
| standard | 4096 | 49.49 | 52.51 | 1.061 |
| standard | 65536 | 142.85 | 151.44 | 1.060 |
| mimalloc | 16 | 6.44 | 10.31 | 1.600 |
| mimalloc | 64 | 6.70 | 10.68 | 1.593 |
| mimalloc | 256 | 7.77 | 11.72 | 1.508 |
| mimalloc | 4096 | 28.74 | 33.06 | 1.150 |
| mimalloc | 65536 | 31.52 | 35.84 | 1.137 |
| jemalloc | 16 | 36.89 | 41.18 | 1.116 |
| jemalloc | 64 | 38.18 | 42.01 | 1.100 |
| jemalloc | 256 | 39.44 | 42.16 | 1.069 |
| jemalloc | 4096 | 43.25 | 45.63 | 1.055 |
| jemalloc | 65536 | 327.56 | 330.99 | 1.010 |

## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


First recorded baseline; no regression comparison is available yet.
