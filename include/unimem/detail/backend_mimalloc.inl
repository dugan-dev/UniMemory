#pragma once

#include <unimem/detail/backend.h>

#include <mimalloc.h>
#include <mimalloc-stats.h>

#include <cstddef>
#include <cstdint>
#include <new>

#if MI_MALLOC_VERSION < 30403
#error "UniMemory requires mimalloc v3.4.3 or newer for native statistics"
#endif

namespace unimem::detail {
namespace mimalloc_impl {

UNIMEMORY_FORCE_INLINE void* default_allocate(void*, std::size_t bytes,
                       std::size_t alignment) noexcept {
    return alignment > alignof(void*)
        ? mi_malloc_aligned(bytes, alignment)
        : mi_malloc(bytes);
}

UNIMEMORY_FORCE_INLINE void* default_allocate_zeroed(void*, std::size_t bytes,
                              std::size_t alignment) noexcept {
    return alignment > alignof(void*)
        ? mi_zalloc_aligned(bytes, alignment)
        : mi_zalloc(bytes);
}

UNIMEMORY_FORCE_INLINE void* default_reallocate(void*, void* pointer, std::size_t,
                         std::size_t bytes, std::size_t alignment) noexcept {
    return mi_realloc_aligned(pointer, bytes, alignment);
}

inline void default_destroy(void*) noexcept {}

UNIMEMORY_FORCE_INLINE void* allocate(void* context, std::size_t bytes,
               std::size_t alignment) noexcept {
    auto* heap = static_cast<mi_heap_t*>(context);
    return alignment > alignof(void*)
        ? mi_heap_malloc_aligned(heap, bytes, alignment)
        : mi_heap_malloc(heap, bytes);
}

UNIMEMORY_FORCE_INLINE void* allocate_zeroed(void* context, std::size_t bytes,
                      std::size_t alignment) noexcept {
    auto* heap = static_cast<mi_heap_t*>(context);
    return alignment > alignof(void*)
        ? mi_heap_zalloc_aligned(heap, bytes, alignment)
        : mi_heap_zalloc(heap, bytes);
}

UNIMEMORY_FORCE_INLINE void* reallocate(void* context, void* pointer, std::size_t,
                 std::size_t bytes, std::size_t alignment) noexcept {
    return mi_heap_realloc_aligned(static_cast<mi_heap_t*>(context),
                                   pointer, bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void deallocate(void*, void* pointer, std::size_t,
                std::size_t) noexcept { mi_free(pointer); }

inline void destroy(void* context) noexcept { mi_heap_destroy(static_cast<mi_heap_t*>(context)); }

inline void* reset(void* context) {
    auto* next = mi_heap_new();
    if (next == nullptr) { throw std::bad_alloc(); }
    destroy(context);
    return next;
}

inline void collect(void* context) { mi_heap_collect(static_cast<mi_heap_t*>(context), true); }

inline bool owns(void* context, const void* pointer) {
    return mi_heap_contains(static_cast<mi_heap_t*>(context), pointer);
}

inline bool statistics(void*, BackendStatistics& result) {
    mi_stats_t_decl(stats);
    if (!mi_stats_get(&stats)) {
        return false;
    }
    const auto nonnegative = [](std::int64_t value) -> std::uint64_t {
        return value < 0 ? 0 : static_cast<std::uint64_t>(value);
    };
    result.scope = BackendStatisticsScope::Process;
    // malloc counters depend on hidden MI_STAT build settings; requested/huge
    // counters in v3.4.3 also lack consistent release updates. Do not report
    // unavailable or cumulative counters as current live allocation bytes.
    result.committed_bytes = nonnegative(stats.committed.current);
    result.reserved_bytes = nonnegative(stats.reserved.current);
    return true;
}

inline const BackendOps ops{allocate, allocate_zeroed, reallocate, deallocate,
                     destroy, nullptr, reset, collect, owns};
inline const BackendOps default_ops{default_allocate, default_allocate_zeroed,
                             default_reallocate, deallocate, default_destroy,
                             statistics};

}

inline BackendHandle mimalloc_backend(bool dedicated) {
    if (!dedicated) { return {&mimalloc_impl::default_ops, nullptr}; }
    auto* heap = mi_heap_new();
    if (heap == nullptr) { throw std::bad_alloc(); }
    return {&mimalloc_impl::ops, heap};
}

inline bool set_mimalloc_release_delay(std::int64_t value) noexcept {
    mi_option_set(mi_option_purge_delay, static_cast<long>(value));
    return mi_option_get(mi_option_purge_delay) == static_cast<long>(value);
}

}
