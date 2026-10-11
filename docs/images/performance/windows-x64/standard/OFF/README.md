# Performance · windows-x64/standard/OFF

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Each operation is an allocation/free pair. Ratios below compare matched Native/API processes in this run; they are not cross-machine rankings.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 45.57 | 46.17 | 1.013 |
| standard | 64 | 44.97 | 46.12 | 1.025 |
| standard | 256 | 47.47 | 48.16 | 1.015 |
| standard | 4096 | 47.51 | 48.96 | 1.030 |
| standard | 65536 | 125.39 | 128.38 | 1.024 |

Profile: `standard` / statistics `OFF` / checks `AUTO`. ON includes bookkeeping feature cost and is not mixed into OFF performance goals. Native and API share one executable, target flags and SDK binaries. Hosted jobs are not controlled cross-host causal comparisons.


## Interpretation

Tail latency includes measured clock overhead. Peak RSS includes stacks and process state. Threads above the recorded CPU capacity are oversubscribed. Timing regressions on hosted runners are signals requiring repeated measurement, not automatic correctness failures.


## Baseline signals

Same recorded CPU/compiler/protocol. These signals require repeated confirmation.

| Backend | Size | API/Native ratio change |
| --- | ---: | ---: |
| standard | 16 | -3.0% |
| standard | 64 | -1.2% |
| standard | 256 | -2.7% |
| standard | 4096 | +0.3% |
| standard | 65536 | +1.4% |
