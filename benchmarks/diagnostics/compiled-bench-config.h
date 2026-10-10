#pragma once
#include <unimem/memory.h>
#include <stdexcept>
inline void compiled_benchmark_configuration(unimem::Backend backend, unimem::StatisticsMode mode) {
    if (backend != unimem::Memory::selected_backend || mode != unimem::Memory::selected_statistics) {
        throw std::invalid_argument("benchmark requires the compiled backend/statistics mode");
    }
}
