---
name: Performance report
about: Report a measured regression or a reproducible workload comparison.
---

## Workload and budget

Describe the application or synthetic trace, relevant time/memory limits and whether the measurement uses native allocation, the UniMemory API or both. State whether counters are enabled.

## Comparable builds

- Baseline and candidate commits or source snapshots:
- CPU, OS, compiler, optimization/LTO and allocator versions:
- Backend, sizes, alignment, thread count and ownership pattern:
- Warmup, repetitions, process trials, affinity and measurement tools:

## Results

Provide exact commands, raw trial data and observed variation. Keep latency, resident memory, requested bytes and allocator statistics distinct. Include failures and regressions as well as improvements. Remove private workload data. The September 2026 tables are historical evidence, not measurements of a later repair snapshot.
