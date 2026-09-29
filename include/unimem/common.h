#pragma once

#include <cstdint>
#include <optional>

namespace unimem {

class Memory;

enum class Backend { Standard, Mimalloc, Jemalloc };

struct BackendCapabilities {
    bool available = false;
    bool detailed_statistics = false;
    bool release_delay = false;
    bool heap = false;
};

bool available(Backend backend) noexcept;
BackendCapabilities capabilities(Backend backend) noexcept;

enum class RuntimeOption { UnusedPageReleaseDelayMs };

bool supports(Backend backend, RuntimeOption option) noexcept;
bool set_runtime_option(Backend backend, RuntimeOption option, std::int64_t value);

enum class MemoryKind { Global, Heap, Stack };

struct MemoryCapabilities {
    bool basic_statistics = false;
    bool detailed_statistics = false;
    bool reset = false;
    bool collect = false;
    bool owns = false;
    bool checkpoints = false;
    bool thread_safe = false;
    bool individual_reclaim = false;
};

enum class StatisticsMode { Disabled, Basic };

struct MemoryStatistics {
    std::uint64_t allocations = 0;
    std::uint64_t deallocations = 0;
    std::uint64_t reallocations = 0;
    std::uint64_t live_bytes = 0;
    std::uint64_t peak_live_bytes = 0;
};

enum class BackendStatisticsScope { Memory, Process };

struct BackendStatistics {
    BackendStatisticsScope scope = BackendStatisticsScope::Memory;
    std::optional<std::uint64_t> requested_bytes;
    std::optional<std::uint64_t> allocated_bytes;
    std::optional<std::uint64_t> committed_bytes;
    std::optional<std::uint64_t> resident_bytes;
    std::optional<std::uint64_t> reserved_bytes;
};

namespace detail {
struct BackendOps;
struct TrackingContext;
}

}
