# Statistics

[Home](../README.md) · [简体中文](statistics.zh-CN.md)

`statistics()` returns request counters for one Global, Heap or Stack. Each instance has its own ledger; Global instances share the native allocator but keep separate counters. Counters exclude direct native calls, backend overhead and process RSS.

## Enable counting

All constructors default to `StatisticsMode::Disabled`. `UNIMEMORY_STATISTICS=ON` permits `StatisticsMode::Basic`; constructing or configuring Basic in an OFF build throws `std::invalid_argument`.

```sh
cmake -S . -B build/basic -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=ON
```

Use C++20, include `<unimem/all.h>` and link `UniMemory::UniMemory`. This example requires the ON configuration:

```cpp
#include <unimem/all.h>
#include <cassert>

int main() {
    unimem::Global memory(unimem::StatisticsMode::Basic);
    {
        auto block = memory.make_block(64);
        const auto during = memory.statistics();
        assert(during.allocations == 1 && during.live_bytes == 64);
    }
    const auto after = memory.statistics();
    assert(after.deallocations == 1 && after.live_bytes == 0);
    assert(after.peak_live_bytes == 64);
    memory.reset_statistics(); // Memory operations are idle
    assert(memory.statistics().allocations == 0);
}
```

## Five fields

All fields are `std::uint64_t` values.

| Field | Meaning |
| --- | --- |
| `allocations` | Successful allocation requests |
| `deallocations` | Successful release requests |
| `reallocations` | Successful resize requests |
| `live_bytes` | Counted requested bytes not yet released |
| `peak_live_bytes` | Peak counted live requested bytes |

Failed requests do not increment counters. Zero-byte allocation and null deallocation are no-ops; reallocating null follows the allocation path, and resizing to zero follows the release path. Counters cover this Memory's owners and container adapters too. Shared-pointer object/control-block layout depends on the standard library: do not assume a specific allocation count for `make_shared`.

## Query, pause and reset

`statistics()` returns a coherent snapshot under this Memory's statistics lock. Disabled returns five zeroes. Counting adds overhead.

Pause Memory operations before `set_statistics(mode)` or `reset_statistics()`; configuration requires an idle instance even when Global/Heap allocation/free are otherwise concurrent. Disabling hides the ledger and pauses new counting; resuming retains its history. Releasing previously counted storage still updates its live-byte bookkeeping, so pausing does not freeze all bookkeeping. Allocations created while Disabled are not retroactively counted.

`reset_statistics()` clears all five fields and starts a new epoch without releasing storage. Pre-reset allocations are excluded from the new live-byte total; freeing them cannot underflow it. Heap/Stack `reset()` releases/reuses storage and clears counters. Stack rewind adjusts live bytes for reclaimed storage and retains operation counts and historical peak.

## Interpret memory use

Stack `used()` reports consumed buffer bytes including padding and storage retained after individual release; `capacity()` is fixed buffer size. A destroyed owner can reduce `live_bytes` while `used()` stays unchanged.

Request counters, native allocator metrics and process RSS measure different quantities; do not add them together. UniMemory exposes no native-statistics snapshot API. Use backend tools for native diagnostics and coordinate activity in their Memory/process scope, including native calls and thread cleanup. Global/Heap support concurrent allocation/free; Stack remains single-threaded. Synchronize application-object access independently of counter snapshots.
