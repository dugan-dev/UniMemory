# Performance · macOS

Source: `81cfacde5cbb1447b088a95438084a132f0a4edd` · [remote run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240)

Statistics OFF. Each row compares Native/API allocation/free pairs from the same measured profile. Backends and ON/OFF profiles use independent builds/runners; hardware and SDK binaries may differ.

| Backend | Size (B) | Native ns/pair | API ns/pair | API / Native |
| --- | ---: | ---: | ---: | ---: |
| standard | 16 | 16.64 | 16.99 | 1.021 |
| standard | 64 | 18.27 | 20.01 | 1.095 |
| standard | 256 | 18.82 | 19.06 | 1.013 |
| standard | 4096 | 22.53 | 22.84 | 1.014 |
| standard | 65536 | 24.59 | 25.01 | 1.017 |
| mimalloc | 16 | 5.81 | 6.15 | 1.059 |
| mimalloc | 64 | 5.88 | 6.09 | 1.036 |
| mimalloc | 256 | 6.97 | 7.45 | 1.069 |
| mimalloc | 4096 | 16.51 | 18.40 | 1.115 |
| mimalloc | 65536 | 17.16 | 17.93 | 1.045 |
| jemalloc | 16 | 20.88 | 17.61 | 0.843 |
| jemalloc | 64 | 18.44 | 24.98 | 1.355 |
| jemalloc | 256 | 19.65 | 17.40 | 0.886 |
| jemalloc | 4096 | 23.13 | 24.19 | 1.046 |
| jemalloc | 65536 | 282.19 | 291.29 | 1.032 |

Tail latency includes clock overhead. Retention shows RSS above each trial's baseline; peak RSS includes process and allocator state. Thread counts above each profile's logical CPU count are oversubscribed.

Full profile provenance, including CPUs, executable and SDK hashes, is retained in [summary.json](summary.json). The statistics chart selects OFF Native/API and ON API; those independent runs do not establish causal statistics overhead.
