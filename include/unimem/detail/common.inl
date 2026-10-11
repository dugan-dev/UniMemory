#pragma once

#include <unimem/detail/configuration.inl>
#include <limits>
#include <stdexcept>
#include <unimem/detail/inline.h>
#include <unimem/detail/backend_system.inl>
#ifdef UNIMEMORY_WITH_MIMALLOC
#include <unimem/detail/backend_mimalloc.inl>
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
#include <unimem/detail/backend_jemalloc.inl>
#endif

namespace unimem {

inline BackendCapabilities capabilities() noexcept {
    if constexpr (detail::build_backend == Backend::Standard) {
        return {};
    } else if constexpr (detail::build_backend == Backend::Mimalloc) {
        return {true, true, true};
    } else {
#ifdef UNIMEMORY_WITH_JEMALLOC
        return {detail::jemalloc_statistics_available(), true, true};
#else
        return {};
#endif
    }
}

inline bool supports(RuntimeOption option) noexcept {
    return option == RuntimeOption::UnusedPageReleaseDelayMs &&
           capabilities().release_delay;
}

inline bool set_runtime_option(RuntimeOption option, std::int64_t value) {
    if (option == RuntimeOption::UnusedPageReleaseDelayMs &&
        (value < -1 ||
         value > static_cast<std::int64_t>((std::numeric_limits<long>::max)()))) {
        throw std::invalid_argument("UniMemory: invalid release delay");
    }
    if (!supports(option)) { return false; }
    bool result = false;
    if constexpr (detail::build_backend == Backend::Mimalloc) {
#ifdef UNIMEMORY_WITH_MIMALLOC
        result = detail::set_mimalloc_release_delay(value);
#endif
    } else if constexpr (detail::build_backend == Backend::Jemalloc) {
#ifdef UNIMEMORY_WITH_JEMALLOC
        result = detail::set_jemalloc_release_delay(value);
#endif
    }
    if (!result) { throw std::runtime_error("UniMemory: backend rejected release delay"); }
    return true;
}

}
