# Performance · linux-x64

Source: `ca4919fe8a5cebb999991458ef7f660a3ead0820` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38027100953)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 16.80 | 21.47 | 1.278 |
| standard | 64 | 16.85 | 21.58 | 1.280 |
| standard | 256 | 16.88 | 21.53 | 1.275 |
| standard | 4096 | 29.63 | 33.39 | 1.127 |
| standard | 65536 | 32.43 | 36.10 | 1.113 |
| mimalloc | 16 | 8.27 | 10.80 | 1.306 |
| mimalloc | 64 | 8.95 | 11.94 | 1.334 |
| mimalloc | 256 | 9.54 | 11.92 | 1.250 |
| mimalloc | 4096 | 24.91 | 25.87 | 1.039 |
| mimalloc | 65536 | 25.68 | 27.39 | 1.066 |
| jemalloc | 16 | 18.89 | 24.22 | 1.282 |
| jemalloc | 64 | 18.86 | 24.29 | 1.288 |
| jemalloc | 256 | 19.40 | 24.49 | 1.263 |
| jemalloc | 4096 | 22.17 | 27.49 | 1.240 |
| jemalloc | 65536 | 371.89 | 375.45 | 1.010 |

## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


## Baseline signals

Same recorded CPU/compiler/protocol. These signals require repeated confirmation.

| Backend | Size | API/Native ratio change |
| --- | ---: | ---: |
| standard | 16 | +0.3% |
| standard | 64 | +0.6% |
| standard | 256 | +0.0% |
| standard | 4096 | +0.1% |
| standard | 65536 | -0.4% |
| mimalloc | 16 | +55.7% |
| mimalloc | 64 | +7.9% |
| mimalloc | 256 | -2.2% |
| mimalloc | 4096 | -9.2% |
| mimalloc | 65536 | -3.5% |
| jemalloc | 16 | +4.6% |
| jemalloc | 64 | +1.7% |
| jemalloc | 256 | -1.4% |
| jemalloc | 4096 | +0.0% |
| jemalloc | 65536 | +0.3% |
