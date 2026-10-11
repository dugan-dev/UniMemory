#pragma once

// Included inside namespace unimem, after the tracking helpers.
UNIMEMORY_FORCE_INLINE bool Memory::is_global() const noexcept {
    // Compare addresses only: Heap/Stack can exist before Global's lifetime.
    return static_cast<const void*>(this) ==
           static_cast<const void*>(detail::global_storage.bytes);
}

template<bool Zero>
UNIMEMORY_FORCE_INLINE void* Memory::global_allocate_raw(
    std::size_t bytes, std::size_t alignment) noexcept(selected_backend != Backend::Standard) {
    if constexpr (selected_backend == Backend::Standard) {
        void* pointer = alignment > alignof(void*)
            ? ::operator new(bytes, std::align_val_t(alignment))
            : ::operator new(bytes);
        if constexpr (Zero) { if (pointer != nullptr) { std::memset(pointer, 0, bytes); } }
        return pointer;
    }
#ifdef UNIMEMORY_WITH_MIMALLOC
    else if constexpr (selected_backend == Backend::Mimalloc) {
        if constexpr (Zero) {
            return alignment > alignof(void*) ? mi_zalloc_aligned(bytes, alignment) : mi_zalloc(bytes);
        } else {
            return alignment > alignof(void*) ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes);
        }
    }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
    else if constexpr (selected_backend == Backend::Jemalloc) {
        return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr
            : je_mallocx(bytes, MALLOCX_ALIGN(alignment) | (Zero ? MALLOCX_ZERO : 0));
    }
#endif
    else { return nullptr; }
}

UNIMEMORY_FORCE_INLINE void Memory::global_deallocate_raw(
    void* pointer, std::size_t alignment) noexcept {
    if constexpr (selected_backend == Backend::Standard) {
        if (alignment > alignof(void*)) { ::operator delete(pointer, std::align_val_t(alignment)); }
        else { ::operator delete(pointer); }
    }
#ifdef UNIMEMORY_WITH_MIMALLOC
    else if constexpr (selected_backend == Backend::Mimalloc) { mi_free(pointer); }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
    else if constexpr (selected_backend == Backend::Jemalloc) { je_dallocx(pointer, 0); }
#endif
}

UNIMEMORY_FORCE_INLINE void* Memory::global_reallocate_raw(
    void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) noexcept(selected_backend != Backend::Standard) {
    if constexpr (selected_backend == Backend::Standard) {
        // Bypass public tracked allocate/free: successful realloc changes only
        // reallocations/live/peak, including when the old/new sizes are equal.
        void* next = global_allocate_raw<false>(new_bytes, alignment);
        if (next != nullptr) {
            std::memcpy(next, pointer, (std::min)(old_bytes, new_bytes));
            global_deallocate_raw(pointer, alignment);
        }
        return next;
    }
#ifdef UNIMEMORY_WITH_MIMALLOC
    else if constexpr (selected_backend == Backend::Mimalloc) {
        return mi_realloc_aligned(pointer, new_bytes, alignment);
    }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
    else if constexpr (selected_backend == Backend::Jemalloc) {
        return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr
            : je_rallocx(pointer, new_bytes, MALLOCX_ALIGN(alignment));
    }
#endif
    else { return nullptr; }
}

// Context operations remain out of line; Global operations inline.
#if defined(_MSC_VER)
#define UNIMEMORY_NOINLINE __declspec(noinline) inline
#elif defined(__GNUC__) || defined(__clang__)
#define UNIMEMORY_NOINLINE inline __attribute__((noinline))
#else
#error "UniMemory requires MSVC, GCC, or Clang noinline support"
#endif

UNIMEMORY_NOINLINE void* Memory::cold_allocate(
    std::size_t bytes, std::size_t alignment) noexcept {
    return ops_->allocate(context_, bytes, alignment);
}

UNIMEMORY_NOINLINE void* Memory::cold_allocate_zeroed(
    std::size_t bytes, std::size_t alignment) noexcept {
    if (ops_->allocate_zeroed != nullptr) {
        return ops_->allocate_zeroed(context_, bytes, alignment);
    }
    void* pointer = ops_->allocate(context_, bytes, alignment);
    if (pointer != nullptr) { std::memset(pointer, 0, bytes); }
    return pointer;
}

UNIMEMORY_NOINLINE void* Memory::cold_reallocate(
    void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) noexcept {
    if (ops_->reallocate != nullptr) {
        return ops_->reallocate(context_, pointer, old_bytes, new_bytes, alignment);
    }
    // Both null/zero and alignment rules are handled by the public caller.
    // These are the same non-Global Ops used by public allocate/deallocate.
    // Tracked Heap Ops already record the original alloc/copy/free semantics;
    // failed allocation does not copy, release, or change the old pointer.
    void* next = ops_->allocate(context_, new_bytes, alignment);
    if (next != nullptr) {
        std::memcpy(next, pointer, (std::min)(old_bytes, new_bytes));
        ops_->deallocate(context_, pointer, old_bytes, alignment);
    }
    return next;
}

UNIMEMORY_NOINLINE void Memory::cold_deallocate(
    void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
    ops_->deallocate(context_, pointer, bytes, alignment);
}

#undef UNIMEMORY_NOINLINE

UNIMEMORY_FORCE_INLINE void* Memory::allocate(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
    if (!memory_impl::valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
#endif
    if (bytes == 0) { return nullptr; }
    const bool global = is_global();
    void* pointer = global ? global_allocate_raw<false>(bytes, alignment)
                          : cold_allocate(bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    if constexpr (selected_statistics == StatisticsMode::Basic) {
        if (global) { memory_impl::record_allocation(tracking_, bytes); }
    }
    return pointer;
}

UNIMEMORY_FORCE_INLINE void* Memory::allocate_zeroed(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
    if (!memory_impl::valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
#endif
    if (bytes == 0) { return nullptr; }
    const bool global = is_global();
    void* pointer = global ? global_allocate_raw<true>(bytes, alignment)
                          : cold_allocate_zeroed(bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    if constexpr (selected_statistics == StatisticsMode::Basic) {
        if (global) { memory_impl::record_allocation(tracking_, bytes); }
    }
    return pointer;
}

UNIMEMORY_FORCE_INLINE void* Memory::reallocate(
    void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
    if (!memory_impl::valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
#endif
    if (pointer == nullptr) { return allocate(new_bytes, alignment); }
    if (new_bytes == 0) { deallocate(pointer, old_bytes, alignment); return nullptr; }
    const bool global = is_global();
    void* next = global ? global_reallocate_raw(pointer, old_bytes, new_bytes, alignment)
                       : cold_reallocate(pointer, old_bytes, new_bytes, alignment);
    if (next == nullptr) { throw std::bad_alloc(); }
    if constexpr (selected_statistics == StatisticsMode::Basic) {
        if (global) { memory_impl::record_reallocation(tracking_, old_bytes, new_bytes); }
    }
    return next;
}

UNIMEMORY_FORCE_INLINE void* Memory::reallocate_zeroed(
    void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) {
    const auto preserved = pointer == nullptr ? 0 : old_bytes;
    void* next = reallocate(pointer, old_bytes, new_bytes, alignment);
    if (next != nullptr && new_bytes > preserved) {
        std::memset(static_cast<std::byte*>(next) + preserved, 0, new_bytes - preserved);
    }
    return next;
}

UNIMEMORY_FORCE_INLINE void Memory::deallocate(
    void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
    if (pointer == nullptr) { return; }
    if (is_global()) {
        global_deallocate_raw(pointer, alignment);
        if constexpr (selected_statistics == StatisticsMode::Basic) {
            memory_impl::record_deallocation(tracking_, bytes);
        }
        return;
    }
    cold_deallocate(pointer, bytes, alignment);
}
