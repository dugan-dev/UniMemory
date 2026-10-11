# Performance · windows-x64

Source: `ca4919fe8a5cebb999991458ef7f660a3ead0820` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38027100953)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 46.58 | 49.32 | 1.059 |
| standard | 64 | 45.41 | 49.91 | 1.099 |
| standard | 256 | 47.04 | 50.90 | 1.082 |
| standard | 4096 | 47.74 | 52.33 | 1.096 |
| standard | 65536 | 143.32 | 149.41 | 1.042 |
| mimalloc | 16 | 6.38 | 10.31 | 1.617 |
| mimalloc | 64 | 6.68 | 10.69 | 1.600 |
| mimalloc | 256 | 7.70 | 11.72 | 1.523 |
| mimalloc | 4096 | 28.74 | 33.08 | 1.151 |
| mimalloc | 65536 | 31.53 | 35.87 | 1.137 |
| jemalloc | 16 | 38.70 | 41.64 | 1.076 |
| jemalloc | 64 | 37.30 | 42.00 | 1.126 |
| jemalloc | 256 | 40.28 | 42.44 | 1.054 |
| jemalloc | 4096 | 42.29 | 45.51 | 1.076 |
| jemalloc | 65536 | 309.33 | 313.10 | 1.012 |

## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


## Baseline signals

Same recorded CPU/compiler/protocol. These signals require repeated confirmation.

| Backend | Size | API/Native ratio change |
| --- | ---: | ---: |
| standard | 16 | -2.9% |
| standard | 64 | -0.7% |
| standard | 256 | +0.4% |
| standard | 4096 | +3.3% |
| standard | 65536 | -1.7% |
| mimalloc | 16 | +1.0% |
| mimalloc | 64 | +0.4% |
| mimalloc | 256 | +1.0% |
| mimalloc | 4096 | +0.0% |
| mimalloc | 65536 | +0.1% |
| jemalloc | 16 | -3.6% |
| jemalloc | 64 | +2.3% |
| jemalloc | 256 | -1.4% |
| jemalloc | 4096 | +2.0% |
| jemalloc | 65536 | +0.2% |
