#include <unimem/memory.h>

#include "hotpath.h"

#include <climits>
#include <new>

#ifdef UNIMEMORY_BENCH_MIMALLOC
#include <mimalloc.h>
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>
#endif

#if defined(_MSC_VER)
#define DIAGNOSTIC_NOINLINE __declspec(noinline)
#else
#define DIAGNOSTIC_NOINLINE __attribute__((noinline))
#endif

namespace unimem_diagnostics {

DIAGNOSTIC_NOINLINE std::size_t opaque_alignment(std::size_t value) noexcept {
    volatile std::size_t observed = value;
    return observed;
}

// Direct and indirect probes invoke these exact same functions.
DIAGNOSTIC_NOINLINE void* shim_standard_allocate(void*, std::size_t bytes, std::size_t alignment) noexcept {
    return alignment > alignof(void*) ? ::operator new(bytes, std::align_val_t(alignment), std::nothrow)
                                     : ::operator new(bytes, std::nothrow);
}
DIAGNOSTIC_NOINLINE void shim_standard_free(void*, void* pointer, std::size_t, std::size_t alignment) noexcept {
    if (alignment > alignof(void*)) { ::operator delete(pointer, std::align_val_t(alignment)); }
    else { ::operator delete(pointer); }
}
DIAGNOSTIC_NOINLINE void* shim_mimalloc_allocate(void*, std::size_t bytes, std::size_t alignment) noexcept {
#ifdef UNIMEMORY_BENCH_MIMALLOC
    return alignment > alignof(void*) ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes);
#else
    (void)bytes; (void)alignment; return nullptr;
#endif
}
DIAGNOSTIC_NOINLINE void shim_mimalloc_free(void*, void* pointer, std::size_t, std::size_t) noexcept {
#ifdef UNIMEMORY_BENCH_MIMALLOC
    mi_free(pointer);
#else
    (void)pointer;
#endif
}
DIAGNOSTIC_NOINLINE void* shim_jemalloc_allocate(void*, std::size_t bytes, std::size_t alignment) noexcept {
#ifdef UNIMEMORY_BENCH_JEMALLOC
    return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr : je_mallocx(bytes, MALLOCX_ALIGN(alignment));
#else
    (void)bytes; (void)alignment; return nullptr;
#endif
}
DIAGNOSTIC_NOINLINE void shim_jemalloc_free(void*, void* pointer, std::size_t, std::size_t) noexcept {
#ifdef UNIMEMORY_BENCH_JEMALLOC
    je_dallocx(pointer, 0);
#else
    (void)pointer;
#endif
}
const unimem::detail::BackendOps& shim_ops(unimem::Backend backend) {
    static const unimem::detail::BackendOps standard{shim_standard_allocate, nullptr, nullptr, shim_standard_free, nullptr, nullptr};
    static const unimem::detail::BackendOps mimalloc{shim_mimalloc_allocate, nullptr, nullptr, shim_mimalloc_free, nullptr, nullptr};
    static const unimem::detail::BackendOps jemalloc{shim_jemalloc_allocate, nullptr, nullptr, shim_jemalloc_free, nullptr, nullptr};
    switch (backend) {
    case unimem::Backend::Standard: return standard;
    case unimem::Backend::Mimalloc: return mimalloc;
    case unimem::Backend::Jemalloc: return jemalloc;
    }
    throw std::invalid_argument("unsupported shim backend");
}

DIAGNOSTIC_NOINLINE void* external_checked_allocate(const unimem::detail::BackendHandle& handle, std::size_t bytes, std::size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) { throw std::invalid_argument("alignment"); }
    if (bytes == 0) { return nullptr; }
    void* pointer = handle.ops->allocate(handle.context, bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    return pointer;
}
DIAGNOSTIC_NOINLINE void external_checked_free(const unimem::detail::BackendHandle& handle, void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
    if (pointer != nullptr) { handle.ops->deallocate(handle.context, pointer, bytes, alignment); }
}
DIAGNOSTIC_NOINLINE void* external_unchecked_allocate(const unimem::detail::BackendHandle& handle, std::size_t bytes, std::size_t alignment) {
    void* pointer = handle.ops->allocate(handle.context, bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    return pointer;
}
DIAGNOSTIC_NOINLINE void external_unchecked_free(const unimem::detail::BackendHandle& handle, void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
    handle.ops->deallocate(handle.context, pointer, bytes, alignment);
}

}
