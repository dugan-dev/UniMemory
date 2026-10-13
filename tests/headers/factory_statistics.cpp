#include "memory_api_contract.h"
#include <array>
#include <memory>

namespace {
struct RawDeleter {
  unimem::Memory* memory;
  std::size_t bytes;
  void operator()(void* pointer) const noexcept { memory->deallocate(pointer, bytes, 1); }
};
using RawOwner = std::unique_ptr<void, RawDeleter>;
}  // namespace

void factory_statistics_regression() {
  using namespace unimem;
  Heap heap = Memory::heap();
  std::array<std::byte, 1024> backing{};
  Stack stack = Memory::stack(backing);
  for (Memory* memory : std::array<Memory*, 2>{&heap, &stack}) {
    RawOwner old(memory->allocate(17, 1), RawDeleter{memory, 17});
    if constexpr (UNIMEMORY_CONFIG_STATISTICS != 0) {
      memory->set_statistics(StatisticsMode::Basic);
    }
    RawOwner tracked(memory->allocate(19, 1), RawDeleter{memory, 19});
    memory->reset_statistics();
    old.reset();
    tracked.reset();
    Statistics statistics = memory->statistics();
    memory->set_statistics(StatisticsMode::Disabled);
    if constexpr (UNIMEMORY_CONFIG_STATISTICS != 0) {
      memory->set_statistics(StatisticsMode::Basic);
    }
    (void)statistics.live_bytes;
  }
}
