#pragma once
#include <unimem/all.h>
#include "headers/memory_api_contract.h"
#include <array>
#include <stdexcept>
namespace compiled_test {
inline constexpr auto backend = unimem::detail::build_backend;
inline constexpr bool supports_statistics = UNIMEMORY_CONFIG_STATISTICS != 0;
inline constexpr auto global_mode = supports_statistics ? unimem::StatisticsMode::Basic : unimem::StatisticsMode::Disabled;
inline constexpr auto heap_mode = global_mode;
inline unimem::StatisticsMode active_mode = unimem::StatisticsMode::Disabled;
inline bool configured = false;
inline bool tracking() {
  return active_mode == unimem::StatisticsMode::Basic;
}
inline bool zero(const unimem::Statistics& value) {
  return value.allocations == 0 && value.deallocations == 0 && value.reallocations == 0 && value.live_bytes == 0 && value.peak_live_bytes == 0;
}
inline void configure(unimem::StatisticsMode mode) {
  active_mode = mode;
  configured = true;
}
inline unimem::Heap heap(unimem::StatisticsMode mode) {
  configure(mode);
  return unimem::Memory::heap(mode);
}
inline auto heap_modes() {
  if constexpr (supports_statistics) {
    return std::array{unimem::StatisticsMode::Disabled, unimem::StatisticsMode::Basic};
  } else {
    return std::array{unimem::StatisticsMode::Disabled};
  }
}
// Forward test configuration to the process-wide Global getter.
inline unimem::Global& global(unimem::StatisticsMode mode = active_mode) {
  return unimem::Memory::global(mode);
}
inline void test_configuration(unimem::detail::Backend requested, unimem::StatisticsMode mode) {
  if (requested != backend) {
    throw std::invalid_argument("compiled backend mismatch");
  }
  configure(mode);
  (void)global(mode);
}
inline void verify_configuration() {
  if (!configured) {
    test_configuration(backend, global_mode);
  }
  auto& memory = global();
  if (memory.kind() != unimem::MemoryKind::Global || &memory != &unimem::Memory::global() ||
      memory.resource() != unimem::Memory::global().resource()) {
    throw std::runtime_error("Global test fixture kind mismatch");
  }
}
}  // namespace compiled_test
