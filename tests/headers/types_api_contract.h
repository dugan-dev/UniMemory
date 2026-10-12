#pragma once
#include <unimem/common.h>
#include <cstdint>
#include <type_traits>

namespace test_api {
static_assert(std::is_enum_v<unimem::MemoryKind>);
static_assert(std::is_enum_v<unimem::StatisticsMode>);
static_assert(std::is_aggregate_v<unimem::Statistics>);
static_assert(std::is_same_v<decltype(unimem::Statistics::allocations), std::uint64_t>);
static_assert(std::is_same_v<decltype(unimem::Statistics::deallocations), std::uint64_t>);
static_assert(std::is_same_v<decltype(unimem::Statistics::reallocations), std::uint64_t>);
static_assert(std::is_same_v<decltype(unimem::Statistics::live_bytes), std::uint64_t>);
static_assert(std::is_same_v<decltype(unimem::Statistics::peak_live_bytes), std::uint64_t>);
}  // namespace test_api
