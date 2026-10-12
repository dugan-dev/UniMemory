# Statistics

[Index](../README.md) · **English** · [简体中文](statistics.zh-CN.md)

Examples: [Quick Start](../../README.md#quick-start).

## Choose a query

| Query | Scope | Content |
| --- | --- | --- |
| `statistics()` | This Memory | Successful operations, requested bytes, peak |
| Stack `used()` | This Buffer | Consumed bytes, including padding and retained storage |

## Counter configuration

`UNIMEMORY_STATISTICS=ON` permits Basic for Global, Heap and Stack; configuring Basic in an OFF build throws `std::invalid_argument`. All modes default to Disabled.

All factories accept `StatisticsMode`, defaulting to Disabled. The process-wide Global captures the mode on its first getter call; later getter arguments do not change it. All callers share its statistics ledger. Use `set_statistics(StatisticsMode::Basic)` to enable an existing instance, and Disabled to pause it; resuming retains its counters.

```sh
cmake -S . -B build/basic -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=ON
```

Counters cover successful requests through this Memory, excluding direct native calls. Heap and Stack reset clear their counters. Enabling statistics adds counting overhead.

| Field | Meaning |
| --- | --- |
| `allocations` / `deallocations` / `reallocations` | Successful operation counts |
| `live_bytes` | Requested bytes not yet released |
| `peak_live_bytes` | Peak live requested bytes |

## When to query

`statistics()` returns a five-field value, all zero when Disabled. Snapshots are coherent within this Memory. Requested bytes, backend native metrics and process RSS measure different things and must not be added together. Use backend tools directly for native diagnostics.

[Backend metric notes](../allocator-capabilities.md) · [Lifetime and threads](../compatibility.md)

`reset_statistics()` clears all five fields and starts a new counting epoch; it does not release storage.

After reset, counters cover the new epoch; freeing pre-reset allocations does not underflow live bytes.
