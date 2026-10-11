# Performance · Windows x64

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 28.71 | 29.27 | 1.020 |
| standard | 64 | 28.76 | 29.56 | 1.028 |
| standard | 256 | 30.96 | 29.39 | 0.949 |
| standard | 4096 | 30.70 | 29.51 | 0.961 |
| standard | 65536 | 85.43 | 89.02 | 1.042 |
| mimalloc | 16 | 7.35 | 7.23 | 0.983 |
| mimalloc | 64 | 6.74 | 7.60 | 1.127 |
| mimalloc | 256 | 7.75 | 8.54 | 1.102 |
| mimalloc | 4096 | 28.74 | 29.35 | 1.021 |
| mimalloc | 65536 | 31.64 | 32.16 | 1.016 |
| jemalloc | 16 | 38.32 | 39.69 | 1.036 |
| jemalloc | 64 | 36.98 | 38.14 | 1.031 |
| jemalloc | 256 | 38.92 | 38.18 | 0.981 |
| jemalloc | 4096 | 41.39 | 43.25 | 1.045 |
| jemalloc | 65536 | 325.77 | 332.09 | 1.019 |

Tail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. Thread counts above each profile's logical CPU count are oversubscribed.

Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.
