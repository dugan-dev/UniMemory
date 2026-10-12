#pragma once

#include <cstdint>

namespace unimem {

enum class MemoryKind { Global, Heap, Stack };
enum class StatisticsMode { Disabled, Basic };

struct Statistics {
  std::uint64_t allocations = 0;
  std::uint64_t deallocations = 0;
  std::uint64_t reallocations = 0;

  // Requested bytes; backend overhead is excluded.
  std::uint64_t live_bytes = 0;
  std::uint64_t peak_live_bytes = 0;
};

}  // namespace unimem
