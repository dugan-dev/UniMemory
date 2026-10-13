#include "compiled-test-config.h"
#include <array>
#include <stdexcept>
#include <type_traits>
static_assert(!std::is_polymorphic_v<unimem::Memory>);
static_assert(!std::has_virtual_destructor_v<unimem::Memory>);
static_assert(!std::is_destructible_v<unimem::Memory>);
int main() {
  using namespace unimem;
  Global global;
  auto global_item = global.make_unique<int>(13);
  auto default_heap = Heap();
  auto default_item = default_heap.make_unique<int>(19);
  std::array<std::byte, 512> bytes{};
  auto default_stack = Stack(bytes);
  auto stack_item = default_stack.make_unique<int>(37);
  for (Memory* memory : std::array<Memory*, 3>{&global, &default_heap, &default_stack}) {
    if (!compiled_test::zero(memory->statistics())) {
      return 1;
    }
  }
  for (auto mode : compiled_test::heap_modes()) {
    Global global_arena(mode);
    auto heap = Heap(mode);
    std::array<std::byte, 512> scratch_bytes{};
    auto stack = Stack(scratch_bytes, mode);
    auto item = heap.make_unique<int>(91);
    auto global_value = global_arena.make_unique<int>(29);
    auto scratch = stack.make_unique<int>(42);
    if (*item != 91 || *scratch != 42 || !heap.owns(item.get())) {
      return 2;
    }
    for (Memory* memory : std::array<Memory*, 3>{&global_arena, &heap, &stack}) {
      const auto recorded = memory->statistics();
      if (mode == StatisticsMode::Basic) {
        if (recorded.allocations != 1 || recorded.live_bytes != sizeof(int)) {
          return 3;
        }
      } else if (!compiled_test::zero(recorded)) {
        return 4;
      }
      memory->set_statistics(StatisticsMode::Disabled);
      if (!compiled_test::zero(memory->statistics())) {
        return 5;
      }
      memory->set_statistics(mode);
      const auto resumed = memory->statistics();
      if (resumed.allocations != recorded.allocations || resumed.deallocations != recorded.deallocations ||
          resumed.reallocations != recorded.reallocations || resumed.live_bytes != recorded.live_bytes ||
          resumed.peak_live_bytes != recorded.peak_live_bytes) {
        return 6;
      }
      memory->reset_statistics();
      if (!compiled_test::zero(memory->statistics())) {
        return 7;
      }
    }
    item.reset();
    global_value.reset();
    scratch.reset();
    if (global_arena.statistics().live_bytes != 0 || heap.statistics().live_bytes != 0 || stack.statistics().live_bytes != 0) {
      return 8;
    }
    Global repeated(mode);
    auto peer_item = repeated.make_unique<int>(23);
    if (&repeated == &global || repeated.resource() == global.resource() || !compiled_test::zero(global.statistics()) ||
        (mode == StatisticsMode::Basic && (repeated.statistics().allocations != 1 || repeated.statistics().live_bytes != sizeof(int))) ||
        (mode == StatisticsMode::Disabled && !compiled_test::zero(repeated.statistics()))) {
      return 9;
    }
  }
  if constexpr (compiled_test::supports_statistics) {
    for (Memory* memory : std::array<Memory*, 3>{&global, &default_heap, &default_stack}) {
      memory->set_statistics(StatisticsMode::Basic);
      if (!compiled_test::zero(memory->statistics())) {
        return 10;
      }
      auto item = memory->make_unique<int>(7);
      if (memory->statistics().allocations != 1 || memory->statistics().live_bytes != sizeof(int)) {
        return 11;
      }
      memory->reset_statistics();
      item.reset();
      if (memory->statistics().live_bytes != 0) {
        return 12;
      }
    }
    global_item.reset();
    default_item.reset();
    stack_item.reset();
    for (Memory* memory : std::array<Memory*, 3>{&global, &default_heap, &default_stack}) {
      if (memory->statistics().live_bytes != 0) {
        return 13;
      }
    }
  } else {
    for (Memory* memory : std::array<Memory*, 3>{&global, &default_heap, &default_stack}) {
      bool rejected = false;
      try {
        memory->set_statistics(StatisticsMode::Basic);
      } catch (const std::invalid_argument&) {
        rejected = true;
      }
      if (!rejected || !compiled_test::zero(memory->statistics())) {
        return 14;
      }
    }
    bool rejected = false;
    try {
      (void)Heap(StatisticsMode::Basic);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    if (!rejected) {
      return 15;
    }
    rejected = false;
    try {
      (void)Stack(bytes, StatisticsMode::Basic);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    if (!rejected) {
      return 16;
    }
    rejected = false;
    try {
      (void)Global(StatisticsMode::Basic);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    if (!rejected || !compiled_test::zero(global.statistics())) {
      return 18;
    }
  }
  bool rejected = false;
  try {
    default_heap.set_statistics(static_cast<StatisticsMode>(99));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  if (!rejected) {
    return 17;
  }
  const auto invalid = static_cast<StatisticsMode>(99);
  auto rejects = [](auto&& operation) {
    try {
      operation();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  if (!rejects([&] { (void)Global(invalid); }) || !rejects([&] { (void)Heap(invalid); }) || !rejects([&] { (void)Stack(bytes, invalid); })) {
    return 19;
  }
  return 0;
}
