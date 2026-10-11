# Performance · Linux x64

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 16.85 | 17.75 | 1.053 |
| standard | 64 | 17.02 | 17.74 | 1.042 |
| standard | 256 | 16.91 | 17.77 | 1.051 |
| standard | 4096 | 29.85 | 30.33 | 1.016 |
| standard | 65536 | 32.41 | 33.20 | 1.024 |
| mimalloc | 16 | 8.61 | 9.22 | 1.070 |
| mimalloc | 64 | 8.97 | 11.87 | 1.323 |
| mimalloc | 256 | 9.49 | 13.31 | 1.402 |
| mimalloc | 4096 | 23.89 | 24.32 | 1.018 |
| mimalloc | 65536 | 25.21 | 25.78 | 1.023 |
| jemalloc | 16 | 19.14 | 19.74 | 1.031 |
| jemalloc | 64 | 19.89 | 19.70 | 0.991 |
| jemalloc | 256 | 18.94 | 19.51 | 1.030 |
| jemalloc | 4096 | 22.05 | 22.72 | 1.030 |
| jemalloc | 65536 | 378.60 | 390.66 | 1.032 |

Tail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. Thread counts above each profile's logical CPU count are oversubscribed.

Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.
