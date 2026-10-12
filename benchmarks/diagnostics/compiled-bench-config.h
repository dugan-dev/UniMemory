#pragma once
#include <unimem/all.h>
#include <stdexcept>
inline void compiled_benchmark_backend(unimem::detail::Backend backend) {
  if (backend != unimem::detail::build_backend) {
    throw std::invalid_argument("benchmark requires the compiled backend");
  }
}
inline void compiled_benchmark_configuration(unimem::detail::Backend backend, unimem::StatisticsMode mode) {
  compiled_benchmark_backend(backend);
  if (mode == unimem::StatisticsMode::Basic && UNIMEMORY_CONFIG_STATISTICS == 0) {
    throw std::invalid_argument("benchmark requires the compiled backend/statistics mode");
  }
  unimem::Memory::global(mode).set_statistics(mode);
}
