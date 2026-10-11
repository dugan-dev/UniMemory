# Performance · Linux x64

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 8.71 | 8.77 | 1.006 |
| standard | 64 | 8.53 | 9.02 | 1.058 |
| standard | 256 | 8.80 | 9.42 | 1.071 |
| standard | 4096 | 15.89 | 15.87 | 0.999 |
| standard | 65536 | 17.28 | 17.37 | 1.005 |
| mimalloc | 16 | 8.38 | 9.40 | 1.122 |
| mimalloc | 64 | 8.83 | 9.45 | 1.070 |
| mimalloc | 256 | 9.75 | 10.20 | 1.046 |
| mimalloc | 4096 | 24.12 | 24.40 | 1.011 |
| mimalloc | 65536 | 25.33 | 25.35 | 1.001 |
| jemalloc | 16 | 11.34 | 11.26 | 0.993 |
| jemalloc | 64 | 10.32 | 10.88 | 1.054 |
| jemalloc | 256 | 10.39 | 10.82 | 1.041 |
| jemalloc | 4096 | 12.87 | 12.92 | 1.004 |
| jemalloc | 65536 | 193.48 | 192.44 | 0.995 |

Tail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. Thread counts above each profile's logical CPU count are oversubscribed.

Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.
