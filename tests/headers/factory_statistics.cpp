#include "memory_api_contract.h"
#include <array>

void factory_statistics_regression() {
  using namespace unimem;
  Heap heap = Memory::heap();
  std::array<std::byte, 1024> backing{};
  Stack stack = Memory::stack(backing);
  for (Memory* memory : std::array<Memory*, 2>{&heap, &stack}) {
    void* old = memory->allocate(17, 1);
    memory->set_statistics(StatisticsMode::Basic);
    void* tracked = memory->allocate(19, 1);
    memory->reset_statistics();
    memory->deallocate(old, 17, 1);
    memory->deallocate(tracked, 19, 1);
    Statistics statistics = memory->statistics();
    memory->set_statistics(StatisticsMode::Disabled);
    memory->set_statistics(StatisticsMode::Basic);
    (void)statistics.live_bytes;
  }
}
