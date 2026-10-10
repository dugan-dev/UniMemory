# Controlled performance diagnosis

> For agentic workers: use systematic-debugging and dispatching-parallel-agents. These are diagnostic probes, not production optimizations.

**Goal:** Identify the cost of individual wrapper, statistics, backend-policy and measurement factors before proposing production changes.

**Architecture:** Opt-in C++20 probes leave production source and public headers unchanged. A Python standard-library runner calibrates common work, randomizes paired fresh-process measurements and preserves raw evidence. GitHub Actions runs all builds and tests on Linux x64, Windows x64 and macOS ARM64.

**Tech Stack:** Existing CMake, pinned mimalloc 3.4.3 and jemalloc 5.3.1, Python standard library, existing GitHub runners.

## Constraints and acceptance

- Work on the existing authorized `dev` branch; do not merge `main`.
- No local C++ compilation, local benchmark execution or dependency changes.
- Preserve READMEs and published charts/data; experiments publish Actions artifacts only.
- Every contrast names its changed factor and matching contract. No omnibus performance score.
- Use at least five randomized process trials, common calibrated operations, approximately 100 ms or more per hot sample, and record min/max plus paired deltas.
- Keep allocations observable, propagate failures, validate allocation alignment/content and counter balance. Thread sinks must be independent.
- Use earliest worker loop start to latest finish for aggregate throughput. Exclude launch/warmup/join; include staggered scheduling during execution and label oversubscription. A synthetic staggered-window regression checks this calculation.
- Never infer nanosecond overhead by subtracting clock quantiles. Tail samples are diagnostic observations, not safety gates.
- RSS includes allocator/OS state. Dedicated heaps require the corresponding native dedicated policy baseline.
- Synthetic statistics variants have diagnostic semantics and cannot silently replace Basic's contract.

## Candidate factors and controls

| Factor | Controlled comparison |
| --- | --- |
| External call boundary | Same checked BackendHandle wrapper, external vs visible inline body |
| Runtime checks | Same inline handle and valid inputs, checked vs unchecked diagnostic body |
| Indirect dispatch | Same adapter function and arguments, direct call vs function pointer |
| Whole wrapper | Matched native throwing contract vs cached actual Memory Disabled |
| Global lookup | Actual cached Memory vs global lookup on every pair |
| Parameter knowledge | Matched constant 64/16 vs runtime loop-invariant size/alignment |
| Native alignment policy | Aligned vs unaligned entry; explicitly different guarantees |
| Standard throwing entry | Throwing vs nothrow aligned new, both translating failure |
| Static/shared boundary | Identical source and optimization, only UniMemory library kind changes |
| LTO/IPO | Identical static or shared build, only IPO changes; fail if unsupported |
| PIC | Identical static non-IPO build, only position-independent code changes |
| Statistics | Native, actual Disabled, actual Basic and synthetic shared/local controls |
| Atomic RMW | Thread-local plain vs per-worker atomic counters, shared atomics vs none |
| Cache-line arrangement | Same shared counters contiguous vs individually 128-byte padded |
| True sharing | Same padded atomic semantics, shared vs per-worker storage |
| Peak high-water update | Same shared counters with vs without peak load/CAS |
| Peak CAS sensitivity | Synthetic reset-to-zero in both controls, only load/CAS differs; artificial stress, not production semantics |
| Live/count updates | Full shared counters vs live-only or counts-only |
| Dedicated policy | Matched native heap/arena vs actual Heap; global vs dedicated is separate |
| Jemalloc tcache | Native global allocation with default tcache vs TCACHE_NONE |
| Purge and decay | Matching native/API collect, delayed collection and runtime option metadata |
| Detailed statistics | Native matching refresh sequence vs API; epoch-only vs cached reads |
| API composition | Matched zero/realloc/unique/array/shared/vector/PMR/RAII workloads |
| Memory touching | Same kernel and allocation pattern, no-touch vs full-touch |
| Timing duration | Same case with short fixed work vs calibrated sustained work |
| Clock overhead | Clock-only samples recorded separately, no percentile subtraction |
| Scheduling capacity | Threads 1/2/4/8/16, record available affinity and quota |
| Allocation lifetime/cache | Hot one-object reuse vs batched live set; native/API matching |
| Warmup/order/state | Fixed warmup, randomized paired order and independent processes |
| Allocator/compiler provenance | Actual versions, flags, binary hashes, library kind and source SHA |
| GNU vs strict adapter mode | Same adapter source, only `CXX_EXTENSIONS` changes; inspect `ffs` vs bit-scan lowering |
| Alignment-flag conversion | Same external jemalloc shim and flags, macro vs C++20 `countr_zero`; verify all supported powers |
| Loop placement | Same GNU probe source with only `-falign-loops=64`; compare constant and runtime kernels and disassembly |

## Work and verification

- [ ] Implement opt-in hot-path probe and external adapter controls; ensure normal library builds are unchanged.
- [ ] Implement independent statistics, backend-policy and matched API probes.
- [ ] Add runner/parser tests for malformed data, missing contrasts and unequal work; run them remotely.
- [ ] Build static, static IPO, shared, shared IPO and static non-PIC on the same platform job. Preserve commands and disassembly.
- [ ] Smoke every selector and run existing correctness tests remotely before interpreting measurements.
- [ ] Repeat all core contrasts, report paired deltas and dispersion. Rerun contradictory signals.
- [ ] Review causal validity independently, document supported findings and limits, then propose improvements to the user.

## Interpretation

Individual differences are contextual, not additive components of a universal total. Inline, dispatch and IPO can interact; statistics storage controls change snapshot semantics. Hosted hardware cannot establish an absolute worst-case bound. Unsupported factors are explicitly recorded rather than reported as zero cost.
