#pragma once

namespace unimem {
namespace detail {
UNIMEMORY_FORCE_INLINE void* global_storage_address() noexcept;
}
UNIMEMORY_FORCE_INLINE bool Memory::is_local_global() const noexcept {
  // Compare addresses only, without reading an unconstructed Global object.
  // A foreign Global (or a different base offset) safely uses the kind fallback.
  return static_cast<const void*>(this) == detail::global_storage_address();
}

template <bool Zero>
UNIMEMORY_FORCE_INLINE void* Memory::global_allocate_raw(std::size_t bytes,
                                                         std::size_t alignment) noexcept(detail::build_backend != detail::Backend::Standard) {
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    if (bytes > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)())) {
      throw std::bad_alloc();
    }
    void* pointer = alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__ ? ::operator new(bytes, std::align_val_t(alignment)) : ::operator new(bytes);
    if constexpr (Zero) {
      if (pointer != nullptr) {
        std::memset(pointer, 0, bytes);
      }
    }
    return pointer;
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    if constexpr (Zero) {
      return alignment > alignof(void*) ? mi_zalloc_aligned(bytes, alignment) : mi_zalloc(bytes);
    } else {
      return alignment > alignof(void*) ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes);
    }
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr : je_mallocx(bytes, MALLOCX_ALIGN(alignment) | (Zero ? MALLOCX_ZERO : 0));
  }
#endif
  else {
    return nullptr;
  }
}

UNIMEMORY_FORCE_INLINE void Memory::global_deallocate_raw(void* pointer, std::size_t alignment) noexcept {
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
      ::operator delete(pointer, std::align_val_t(alignment));
    } else {
      ::operator delete(pointer);
    }
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    mi_free(pointer);
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    je_dallocx(pointer, 0);
  }
#endif
}

UNIMEMORY_FORCE_INLINE void* Memory::global_reallocate_raw(void* pointer, std::size_t old_bytes, std::size_t new_bytes,
                                                           std::size_t alignment) noexcept(detail::build_backend != detail::Backend::Standard) {
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    void* next = global_allocate_raw<false>(new_bytes, alignment);
    if (next != nullptr) {
      std::memcpy(next, pointer, (std::min)(old_bytes, new_bytes));
      global_deallocate_raw(pointer, alignment);
    }
    return next;
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    return mi_realloc_aligned(pointer, new_bytes, alignment);
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr : je_rallocx(pointer, new_bytes, MALLOCX_ALIGN(alignment));
  }
#endif
  else {
    return nullptr;
  }
}

// Only the three supported concrete classes can construct Memory. The immutable
// kind therefore identifies the derived storage even across library modules.
template <bool Zero>
UNIMEMORY_NOINLINE inline void* Memory::allocate_cold(std::size_t bytes,
                                                      std::size_t alignment) noexcept(detail::build_backend != detail::Backend::Standard) {
  if (kind_ == MemoryKind::Global) {
    return global_allocate_raw<Zero>(bytes, alignment);
  }
  if (kind_ == MemoryKind::Heap) {
    return static_cast<Heap&>(*this).allocate_raw<Zero>(bytes, alignment);
  }
  void* pointer = Stack::stack_allocate(&static_cast<Stack&>(*this).storage_, bytes, alignment);
  if constexpr (Zero) {
    if (pointer != nullptr) {
      std::memset(pointer, 0, bytes);
    }
  }
  return pointer;
}

UNIMEMORY_NOINLINE inline void* Memory::reallocate_cold(void* pointer, std::size_t old_bytes, std::size_t new_bytes,
                                                        std::size_t alignment) noexcept(detail::build_backend != detail::Backend::Standard) {
  if (kind_ == MemoryKind::Global) {
    return global_reallocate_raw(pointer, old_bytes, new_bytes, alignment);
  }
  if (kind_ == MemoryKind::Heap) {
    return static_cast<Heap&>(*this).reallocate_raw(pointer, old_bytes, new_bytes, alignment);
  }
  return Stack::stack_reallocate(&static_cast<Stack&>(*this).storage_, pointer, old_bytes, new_bytes, alignment);
}

UNIMEMORY_NOINLINE inline void Memory::deallocate_cold(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
  if (kind_ == MemoryKind::Global) {
    global_deallocate_raw(pointer, alignment);
    return;
  }
  if (kind_ == MemoryKind::Heap) {
    static_cast<Heap&>(*this).deallocate_raw(pointer, bytes, alignment);
  }
}

template <bool Zero>
UNIMEMORY_FORCE_INLINE void* Memory::allocate_impl(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
  if (!memory_impl::valid_alignment(alignment)) {
    throw std::invalid_argument("UniMemory: alignment must be a power of two");
  }
#endif
  if (bytes == 0) {
    return nullptr;
  }
  void* pointer;
  if constexpr (detail::build_statistics == StatisticsMode::Basic) {
    if (tracking_ != nullptr) {
      pointer = memory_impl::tracked_allocate_impl<Zero>(tracking_, bytes, alignment);
    } else {
      pointer = is_local_global() ? global_allocate_raw<Zero>(bytes, alignment) : allocate_cold<Zero>(bytes, alignment);
    }
  } else {
    pointer = is_local_global() ? global_allocate_raw<Zero>(bytes, alignment) : allocate_cold<Zero>(bytes, alignment);
  }
  if (pointer == nullptr) {
    throw std::bad_alloc();
  }
  return pointer;
}

UNIMEMORY_FORCE_INLINE void* Memory::allocate(std::size_t bytes, std::size_t alignment) {
  return allocate_impl<false>(bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void* Memory::allocate_zeroed(std::size_t bytes, std::size_t alignment) {
  return allocate_impl<true>(bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void* Memory::reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
  if (!memory_impl::valid_alignment(alignment)) {
    throw std::invalid_argument("UniMemory: alignment must be a power of two");
  }
#endif
  if (pointer == nullptr) {
    return allocate(new_bytes, alignment);
  }
  if (new_bytes == 0) {
    deallocate(pointer, old_bytes, alignment);
    return nullptr;
  }
  void* next;
  if constexpr (detail::build_statistics == StatisticsMode::Basic) {
    if (tracking_ != nullptr) {
      next = memory_impl::tracked_reallocate(tracking_, pointer, old_bytes, new_bytes, alignment);
    } else {
      next = is_local_global() ? global_reallocate_raw(pointer, old_bytes, new_bytes, alignment)
                               : reallocate_cold(pointer, old_bytes, new_bytes, alignment);
    }
  } else {
    next = is_local_global() ? global_reallocate_raw(pointer, old_bytes, new_bytes, alignment)
                             : reallocate_cold(pointer, old_bytes, new_bytes, alignment);
  }
  if (next == nullptr) {
    throw std::bad_alloc();
  }
  return next;
}

UNIMEMORY_FORCE_INLINE void* Memory::reallocate_zeroed(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) {
  const auto preserved = pointer == nullptr ? 0 : old_bytes;
  void* next = reallocate(pointer, old_bytes, new_bytes, alignment);
  if (next != nullptr && new_bytes > preserved) {
    std::memset(static_cast<std::byte*>(next) + preserved, 0, new_bytes - preserved);
  }
  return next;
}

UNIMEMORY_FORCE_INLINE void Memory::deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
  if (pointer == nullptr) {
    return;
  }
  if constexpr (detail::build_statistics == StatisticsMode::Basic) {
    if (tracking_ != nullptr) {
      memory_impl::tracked_deallocate(tracking_, pointer, bytes, alignment);
      return;
    }
  }
  if (is_local_global()) {
    global_deallocate_raw(pointer, alignment);
  } else {
    deallocate_cold(pointer, bytes, alignment);
  }
}

}  // namespace unimem
