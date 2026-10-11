# Performance · Windows x64

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 45.57 | 46.17 | 1.013 |
| standard | 64 | 44.97 | 46.12 | 1.025 |
| standard | 256 | 47.47 | 48.16 | 1.015 |
| standard | 4096 | 47.51 | 48.96 | 1.030 |
| standard | 65536 | 125.39 | 128.38 | 1.024 |
| mimalloc | 16 | 7.34 | 7.24 | 0.986 |
| mimalloc | 64 | 6.84 | 7.56 | 1.106 |
| mimalloc | 256 | 7.82 | 8.54 | 1.093 |
| mimalloc | 4096 | 28.74 | 29.36 | 1.022 |
| mimalloc | 65536 | 31.52 | 32.15 | 1.020 |
| jemalloc | 16 | 32.87 | 33.53 | 1.020 |
| jemalloc | 64 | 33.23 | 32.99 | 0.993 |
| jemalloc | 256 | 33.37 | 33.65 | 1.008 |
| jemalloc | 4096 | 35.90 | 36.52 | 1.017 |
| jemalloc | 65536 | 289.72 | 289.63 | 1.000 |

Tail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. Thread counts above each profile's logical CPU count are oversubscribed.

Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.
