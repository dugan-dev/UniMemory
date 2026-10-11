#pragma once

#include <unimem/detail/backend.h>

#include <new>
#include <stdexcept>

namespace unimem::detail {
namespace system_impl {

// A linked allocator may override global new and provide only pointer alignment
// for tiny allocations. Use aligned new whenever the requested guarantee is
// stronger than pointer alignment.
inline constexpr std::size_t default_new_alignment = alignof(void*);

UNIMEMORY_FORCE_INLINE void* allocate(void*, std::size_t bytes, std::size_t alignment) noexcept {
    if (alignment > default_new_alignment) {
        return ::operator new(bytes, std::align_val_t(alignment), std::nothrow);
    }
    return ::operator new(bytes, std::nothrow);
}

UNIMEMORY_FORCE_INLINE void deallocate(void*, void* pointer, std::size_t bytes,
                std::size_t alignment) noexcept {
    (void)bytes;
    if (alignment > default_new_alignment) {
        ::operator delete(pointer, std::align_val_t(alignment));
    } else {
        ::operator delete(pointer);
    }
}

inline void destroy(void*) noexcept {}
inline const BackendOps ops{allocate, nullptr, nullptr, deallocate, destroy, nullptr};

}

inline BackendHandle system_backend(bool dedicated) {
    if (dedicated) {
        throw std::runtime_error("UniMemory: Standard does not support independent Heap");
    }
    return {&system_impl::ops, nullptr};
}

}
