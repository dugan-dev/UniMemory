#pragma once
#include <unimem/memory.h>
#include <stdexcept>
inline void compiled_benchmark_backend(unimem::Backend backend) {
    if (backend != unimem::detail::build_backend) {
        throw std::invalid_argument("benchmark requires the compiled backend");
    }
}
inline void compiled_benchmark_configuration(unimem::Backend backend, unimem::StatisticsMode mode) {
    compiled_benchmark_backend(backend);
    if (mode != unimem::detail::build_statistics) {
        throw std::invalid_argument("benchmark requires the compiled backend/statistics mode");
    }
}
