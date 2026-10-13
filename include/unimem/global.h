#pragma once

#include <unimem/memory.h>
#include <unimem/detail/members.h>

namespace unimem {

// Shared allocator with per-object statistics.
class Global final : public Memory {
 public:
  Global(StatisticsMode statistics = StatisticsMode::Disabled);
  ~Global() = default;
};

}  // namespace unimem
