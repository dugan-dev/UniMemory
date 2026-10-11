#pragma once

#include <unimem/detail/backend.h>


#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4068)
#endif
#ifndef JEMALLOC_NO_RENAME
#define JEMALLOC_NO_RENAME
#define UNIMEMORY_RESTORE_JEMALLOC_RENAME
#endif
#include <jemalloc/jemalloc.h>
#ifdef UNIMEMORY_RESTORE_JEMALLOC_RENAME
#undef JEMALLOC_NO_RENAME
#undef UNIMEMORY_RESTORE_JEMALLOC_RENAME
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace unimem::detail {
namespace jemalloc_impl {

struct ArenaContext { unsigned index; };

inline bool initialize() noexcept {
    // Windows jemalloc can race its process-wide TSD bootstrap when the first
    // calls come from different workers. Finish it before exposing the backend.
    static std::once_flag initialized;
    try {
        std::call_once(initialized, [] {
            const char* version = nullptr;
            std::size_t length = sizeof(version);
            if (je_mallctl("version", &version, &length, nullptr, 0) != 0 ||
                length != sizeof(version) || version == nullptr) {
                throw std::runtime_error("UniMemory: jemalloc initialization failed");
            }
        });
        return true;
    } catch (...) {
        // A failed call_once remains retryable, including after allocation failure.
        return false;
    }
}

UNIMEMORY_FORCE_INLINE int arena_flags(void* context) noexcept {
    return MALLOCX_ARENA(static_cast<ArenaContext*>(context)->index) |
           MALLOCX_TCACHE_NONE;
}

UNIMEMORY_FORCE_INLINE int flags_for(void* context, std::size_t alignment) noexcept {
    return MALLOCX_ALIGN(alignment) |
           (context == nullptr ? 0 : arena_flags(context));
}

UNIMEMORY_FORCE_INLINE void* allocate(void* context, std::size_t bytes,
               std::size_t alignment) noexcept {
    return alignment > static_cast<std::size_t>(INT_MAX)
        ? nullptr : je_mallocx(bytes, flags_for(context, alignment));
}

UNIMEMORY_FORCE_INLINE void* allocate_zeroed(void* context, std::size_t bytes,
                      std::size_t alignment) noexcept {
    return alignment > static_cast<std::size_t>(INT_MAX)
        ? nullptr : je_mallocx(bytes, flags_for(context, alignment) | MALLOCX_ZERO);
}

UNIMEMORY_FORCE_INLINE void* reallocate(void* context, void* pointer, std::size_t,
                 std::size_t bytes, std::size_t alignment) noexcept {
    return alignment > static_cast<std::size_t>(INT_MAX)
        ? nullptr : je_rallocx(pointer, bytes, flags_for(context, alignment));
}

UNIMEMORY_FORCE_INLINE void deallocate(void* context, void* pointer, std::size_t,
                std::size_t) noexcept {
    je_dallocx(pointer, context == nullptr ? 0 : MALLOCX_TCACHE_NONE);
}

inline void destroy(void*) noexcept {}

inline bool read_size(const char* name, std::size_t& value) noexcept {
    std::size_t length = sizeof(value);
    return je_mallctl(name, &value, &length, nullptr, 0) == 0 &&
           length == sizeof(value);
}

inline bool read_arena_size(unsigned index, const char* suffix,
                     std::size_t& value) noexcept {
    char name[96];
    const auto count = std::snprintf(name, sizeof(name),
                                     "stats.arenas.%u.%s", index, suffix);
    return count > 0 && static_cast<std::size_t>(count) < sizeof(name) &&
           read_size(name, value);
}

inline bool statistics(void* context, BackendStatistics& result) {
    bool enabled = false;
    std::size_t enabled_size = sizeof(enabled);
    if (je_mallctl("config.stats", &enabled, &enabled_size, nullptr, 0) != 0 ||
        !enabled) { return false; }
    std::uint64_t epoch = 1;
    if (je_mallctl("epoch", nullptr, nullptr, &epoch, sizeof(epoch)) != 0) {
        return false;
    }
    std::size_t value = 0;
    if (context == nullptr) {
        result.scope = BackendStatisticsScope::Process;
        if (!read_size("stats.allocated", value)) { return false; }
        result.allocated_bytes = value;
        if (read_size("stats.resident", value)) { result.resident_bytes = value; }
    } else {
        result.scope = BackendStatisticsScope::Memory;
        const auto index = static_cast<ArenaContext*>(context)->index;
        std::size_t small_allocated = 0;
        std::size_t large_allocated = 0;
        if (!read_arena_size(index, "small.allocated", small_allocated) ||
            !read_arena_size(index, "large.allocated", large_allocated)) { return false; }
        if (large_allocated > (std::numeric_limits<std::size_t>::max)() - small_allocated) { return false; }
        result.allocated_bytes = small_allocated + large_allocated;
        if (read_arena_size(index, "resident", value)) {
            result.resident_bytes = value;
        }
    }
    return true;
}

inline void destroy_arena(void* context) noexcept {
    auto* arena = static_cast<ArenaContext*>(context);
    char name[64];
    const auto count = std::snprintf(name, sizeof(name),
                                     "arena.%u.destroy", arena->index);
    if (count > 0 && static_cast<std::size_t>(count) < sizeof(name)) {
        (void)je_mallctl(name, nullptr, nullptr, nullptr, 0);
    }
    delete arena;
}

inline void arena_command(void* context, const char* command) {
    char name[64];
    const auto count = std::snprintf(name, sizeof(name), "arena.%u.%s",
        static_cast<ArenaContext*>(context)->index, command);
    if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name) ||
        je_mallctl(name, nullptr, nullptr, nullptr, 0) != 0) {
        throw std::runtime_error("UniMemory: jemalloc arena command failed");
    }
}

inline void* reset(void* context) {
    arena_command(context, "reset");
    return context;
}

inline void collect(void* context) { arena_command(context, "purge"); }

inline bool owns(void* context, const void* pointer) {
    unsigned index = 0;
    std::size_t length = sizeof(index);
    void* allocation = const_cast<void*>(pointer);
    if (je_mallctl("arenas.lookup", &index, &length,
                   &allocation, sizeof(allocation)) != 0 || length != sizeof(index)) {
        throw std::runtime_error("UniMemory: jemalloc allocation lookup failed");
    }
    return index == static_cast<ArenaContext*>(context)->index;
}

inline const BackendOps ops{allocate, allocate_zeroed, reallocate, deallocate,
                     destroy, statistics};
inline const BackendOps arena_ops{allocate, allocate_zeroed, reallocate, deallocate,
                           destroy_arena, statistics, reset, collect, owns};

}

inline BackendHandle jemalloc_backend(bool dedicated) {
    if (!jemalloc_impl::initialize()) {
        throw std::runtime_error("UniMemory: jemalloc initialization failed");
    }
    if (!dedicated) { return {&jemalloc_impl::ops, nullptr}; }
    auto* arena = new jemalloc_impl::ArenaContext{};
    std::size_t length = sizeof(arena->index);
    if (je_mallctl("arenas.create", &arena->index, &length, nullptr, 0) != 0 ||
        length != sizeof(arena->index)) {
        delete arena;
        throw std::bad_alloc();
    }
    constexpr unsigned max_index = (1u << (sizeof(int) * CHAR_BIT - 21)) - 2u;
    if (arena->index > max_index) {
        jemalloc_impl::destroy_arena(arena);
        throw std::runtime_error("UniMemory: jemalloc arena index exceeds flag range");
    }
    return {&jemalloc_impl::arena_ops, arena};
}

inline bool jemalloc_statistics_available() noexcept {
    if (!jemalloc_impl::initialize()) { return false; }
    bool enabled = false;
    std::size_t length = sizeof(enabled);
    return je_mallctl("config.stats", &enabled, &length, nullptr, 0) == 0 &&
           enabled;
}

inline bool set_jemalloc_release_delay(std::int64_t value) noexcept {
    if (!jemalloc_impl::initialize()) { return false; }
    using SignedSize = std::make_signed_t<std::size_t>;
    if (value > static_cast<std::int64_t>((std::numeric_limits<SignedSize>::max)())) {
        return false;
    }
    auto next = static_cast<SignedSize>(value);
    SignedSize old_dirty = 0;
    SignedSize old_muzzy = 0;
    std::size_t length = sizeof(SignedSize);
    if (je_mallctl("arenas.dirty_decay_ms", &old_dirty, &length,
                   nullptr, 0) != 0 || length != sizeof(SignedSize)) { return false; }
    length = sizeof(SignedSize);
    if (je_mallctl("arenas.muzzy_decay_ms", &old_muzzy, &length,
                   nullptr, 0) != 0 || length != sizeof(SignedSize)) { return false; }
    if (je_mallctl("arenas.dirty_decay_ms", nullptr, nullptr,
                   &next, sizeof(next)) != 0) { return false; }
    if (je_mallctl("arenas.muzzy_decay_ms", nullptr, nullptr,
                   &next, sizeof(next)) != 0) {
        (void)je_mallctl("arenas.dirty_decay_ms", nullptr, nullptr,
                         &old_dirty, sizeof(old_dirty));
        return false;
    }
    return true;
}

}
