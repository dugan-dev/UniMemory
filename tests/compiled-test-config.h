#pragma once
#include <unimem/memory.h>
#include "headers/memory_api_contract.h"
#include <array>
#include <stdexcept>
namespace compiled_test {
inline constexpr auto backend = unimem::detail::build_backend;
inline constexpr auto global_mode = unimem::detail::build_statistics;
inline constexpr bool supports_statistics = global_mode == unimem::StatisticsMode::Basic;
inline constexpr auto heap_mode = supports_statistics ? unimem::StatisticsMode::Basic : unimem::StatisticsMode::Disabled;
inline auto heap_modes() {
    if constexpr (supports_statistics) {
        return std::array{unimem::StatisticsMode::Disabled, unimem::StatisticsMode::Basic};
    } else { return std::array{unimem::StatisticsMode::Disabled}; }
}
inline void test_configuration(unimem::Backend requested, unimem::StatisticsMode mode) {
    if (requested != backend) { throw std::invalid_argument("compiled backend mismatch"); }
    if (mode != unimem::StatisticsMode::Basic && mode != unimem::StatisticsMode::Disabled) {
        throw std::invalid_argument("invalid statistics mode");
    }
    if (mode != global_mode) { throw std::logic_error("compiled statistics mismatch"); }
}
inline void verify_configuration() {
    auto& memory = unimem::Memory::global();
    if (&memory != &unimem::Memory::global() || memory.backend() != backend ||
        memory.statistics().has_value() != supports_statistics) {
        throw std::runtime_error("fixed Global configuration/identity mismatch");
    }
}
}
