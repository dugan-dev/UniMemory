# Measurements and validation

[Documentation](../../README.md) · [Performance](../../performance.md) · [Tests](../../testing.md)

Version **0.0.1**. CSVs retain every process trial; reports show medians. SHA-256 hashes identify the measured benchmark sources and executables. Text source hashes use LF line endings.

| Folder / file | Content |
| --- | --- |
| windows/, linux/ · latency.csv | Native/API/Basic, five sizes, three trials, within-process min/median/max |
| windows/, linux/ · full.csv | 1001 API scenarios × three trials; seven-repetition medians |
| windows/, linux/ · pressure.csv | Mixed sizes, five phases, content checks and resident memory |
| windows/, linux/ · footprint.csv | Fixed-size live/free/collect snapshots |
| windows/, linux/ · heap.csv | owns, collect and reset timings |
| windows/, linux/ · environment.json | Delivery benchmark environment, versions, hashes, type sizes |
| windows/, linux/ · sweep-environment.json | Sweep executable/source hashes and configuration |
| native-applications/ | Separate Linux native programs; no UniMemory wrapper |

Windows uses the working set; Linux snapshots use smaps_rollup RSS. Native program peak RSS uses GNU time; elapsed time uses a monotonic clock. These memory fields measure different scopes. Windows jemalloc headers have a placeholder version string; package metadata verifies 5.3.1. Environment metadata uses portable file names rather than local absolute paths.

[Reproduce measurements](../../benchmarking.md) · [Reproduce native suites](../../upstream-validation.md)
