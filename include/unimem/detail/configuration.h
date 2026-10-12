#pragma once

#include <unimem/detail/config.inl>

namespace unimem::detail {

enum class Backend { Standard, Mimalloc, Jemalloc };

inline constexpr Backend build_backend = static_cast<Backend>(UNIMEMORY_CONFIG_BACKEND);
inline constexpr StatisticsMode build_statistics = UNIMEMORY_CONFIG_STATISTICS ? StatisticsMode::Basic : StatisticsMode::Disabled;

struct BackendOps;
struct BackendHandle;
struct TrackingContext;

}  // namespace unimem::detail

#if defined(_MSC_VER)
#define UNIMEMORY_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define UNIMEMORY_FORCE_INLINE inline __attribute__((always_inline))
#else
#define UNIMEMORY_FORCE_INLINE inline
#endif
