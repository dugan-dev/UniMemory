#pragma once

#include <cstddef>
#include <cstdint>

namespace unimem { struct BackendStatistics; }

namespace unimem::detail {

struct BackendOps {
    void* (*allocate)(void*, std::size_t, std::size_t) noexcept;
    void* (*allocate_zeroed)(void*, std::size_t, std::size_t) noexcept;
    void* (*reallocate)(void*, void*, std::size_t, std::size_t,
                        std::size_t) noexcept;
    void (*deallocate)(void*, void*, std::size_t, std::size_t) noexcept;
    void (*destroy)(void*) noexcept;
    bool (*statistics)(void*, BackendStatistics&);
    void* (*reset)(void*) = nullptr;
    void (*collect)(void*) = nullptr;
    bool (*owns)(void*, const void*) = nullptr;
};

struct BackendHandle {
    const BackendOps* ops;
    void* context;
};

BackendHandle system_backend(bool dedicated);
bool set_mimalloc_release_delay(std::int64_t value) noexcept;
bool set_jemalloc_release_delay(std::int64_t value) noexcept;

#ifdef UNIMEMORY_WITH_MIMALLOC
BackendHandle mimalloc_backend(bool dedicated);
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
BackendHandle jemalloc_backend(bool dedicated);
bool jemalloc_statistics_available() noexcept;
#endif

}
