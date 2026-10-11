#pragma once

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

inline bool available(Backend backend) noexcept {
    return backend == static_cast<Backend>(UNIMEMORY_CONFIG_BACKEND);
}

inline BackendCapabilities capabilities(Backend backend) noexcept {
    if (!available(backend)) { return {}; }
    switch (backend) {
    case Backend::Standard: return {true, false, false, false};
    case Backend::Mimalloc: return {true, true, true, true};
    case Backend::Jemalloc:
#ifdef UNIMEMORY_WITH_JEMALLOC
        return {true, detail::jemalloc_statistics_available(), true, true};
#else
        return {};
#endif
    default: return {};
    }
}

inline bool supports(Backend backend, RuntimeOption option) noexcept {
    return option == RuntimeOption::UnusedPageReleaseDelayMs &&
           capabilities(backend).release_delay;
}

inline bool set_runtime_option(Backend backend, RuntimeOption option, std::int64_t value) {
    if (option == RuntimeOption::UnusedPageReleaseDelayMs &&
        (value < -1 ||
         value > static_cast<std::int64_t>((std::numeric_limits<long>::max)()))) {
        throw std::invalid_argument("UniMemory: invalid release delay");
    }
    if (!supports(backend, option)) { return false; }
    bool result = false;
    switch (backend) {
    case Backend::Mimalloc:
#ifdef UNIMEMORY_WITH_MIMALLOC
        result = detail::set_mimalloc_release_delay(value);
#endif
        break;
    case Backend::Jemalloc:
#ifdef UNIMEMORY_WITH_JEMALLOC
        result = detail::set_jemalloc_release_delay(value);
#endif
        break;
    default: break;
    }
    if (!result) { throw std::runtime_error("UniMemory: backend rejected release delay"); }
    return true;
}

}
