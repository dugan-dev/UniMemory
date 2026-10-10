# Performance · macos-arm64

Source: `ca4919fe8a5cebb999991458ef7f660a3ead0820` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38027100953)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 41.24 | 43.98 | 1.066 |
| standard | 64 | 41.62 | 49.36 | 1.186 |
| standard | 256 | 53.42 | 49.13 | 0.920 |
| standard | 4096 | 50.20 | 54.22 | 1.080 |
| standard | 65536 | 347.31 | 337.34 | 0.971 |
| mimalloc | 16 | 5.08 | 7.35 | 1.448 |
| mimalloc | 64 | 5.43 | 7.24 | 1.335 |
| mimalloc | 256 | 6.37 | 9.25 | 1.452 |
| mimalloc | 4096 | 14.80 | 17.05 | 1.152 |
| mimalloc | 65536 | 18.95 | 22.55 | 1.190 |
| jemalloc | 16 | 15.54 | 19.11 | 1.229 |
| jemalloc | 64 | 17.07 | 23.41 | 1.371 |
| jemalloc | 256 | 16.60 | 22.66 | 1.365 |
| jemalloc | 4096 | 29.55 | 24.20 | 0.819 |
| jemalloc | 65536 | 328.56 | 288.75 | 0.879 |

## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


## Baseline signals

Same recorded CPU/compiler/protocol. These signals require repeated confirmation.

| Backend | Size | API/Native ratio change |
| --- | ---: | ---: |
| standard | 16 | -2.1% |
| standard | 64 | +8.3% |
| standard | 256 | -14.1% |
| standard | 4096 | +0.4% |
| standard | 65536 | -4.1% |
| mimalloc | 16 | +7.1% |
| mimalloc | 64 | -3.0% |
| mimalloc | 256 | +10.6% |
| mimalloc | 4096 | +5.6% |
| mimalloc | 65536 | +8.5% |
| jemalloc | 16 | +9.0% |
| jemalloc | 64 | +21.0% |
| jemalloc | 256 | +21.8% |
| jemalloc | 4096 | -26.8% |
| jemalloc | 65536 | -14.0% |
