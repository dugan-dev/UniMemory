# Performance · macOS

Source: `e758dc2a847603b8fbd0cd3125f69012388aaf82` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387)

Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 16.46 | 18.90 | 1.149 |
| standard | 64 | 19.56 | 20.78 | 1.062 |
| standard | 256 | 19.54 | 20.69 | 1.059 |
| standard | 4096 | 22.34 | 22.53 | 1.009 |
| standard | 65536 | 27.36 | 25.41 | 0.929 |
| mimalloc | 16 | 4.96 | 5.34 | 1.077 |
| mimalloc | 64 | 5.19 | 5.73 | 1.104 |
| mimalloc | 256 | 6.82 | 6.81 | 0.998 |
| mimalloc | 4096 | 14.72 | 15.25 | 1.036 |
| mimalloc | 65536 | 16.63 | 16.68 | 1.003 |
| jemalloc | 16 | 15.17 | 16.69 | 1.100 |
| jemalloc | 64 | 15.99 | 21.26 | 1.329 |
| jemalloc | 256 | 16.12 | 15.62 | 0.969 |
| jemalloc | 4096 | 22.29 | 20.83 | 0.935 |
| jemalloc | 65536 | 248.32 | 262.33 | 1.056 |

Tail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. Thread counts above each profile's logical CPU count are oversubscribed.

Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.
