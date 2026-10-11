#pragma once

#include <unimem/detail/config.inl>

namespace unimem::detail {

inline constexpr Backend build_backend = static_cast<Backend>(UNIMEMORY_CONFIG_BACKEND);
inline constexpr StatisticsMode build_statistics = UNIMEMORY_CONFIG_STATISTICS
    ? StatisticsMode::Basic : StatisticsMode::Disabled;

struct BackendOps;
struct TrackingContext;

}
