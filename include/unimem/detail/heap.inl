#pragma once

namespace unimem {

inline Heap::Heap(StatisticsMode statistics) : Memory(MemoryKind::Heap) {
  memory_impl::validate_statistics(statistics);
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    heap_ = detail::system_backend(true);
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    heap_ = detail::mimalloc_backend(true);
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    heap_ = detail::jemalloc_backend(true);
  }
#endif
  try {
    set_statistics(statistics);
  } catch (...) {
    heap_.ops->destroy(heap_.context);
    heap_ = {};
    throw;
  }
}

inline Heap::~Heap() {
  heap_.ops->destroy(heap_.context);
}

inline void Heap::reset() {
  heap_.context = heap_.ops->reset(heap_.context);
  if (tracking_ != nullptr) {
    tracking_->base = heap_;
  }
  clear_allocation_statistics();
}

inline void Heap::collect() {
  heap_.ops->collect(heap_.context);
}
inline bool Heap::owns(const void* pointer) const {
  return pointer != nullptr && heap_.ops->owns(heap_.context, pointer);
}

template <bool Zero>
UNIMEMORY_FORCE_INLINE void* Heap::allocate_raw(std::size_t bytes, std::size_t alignment) noexcept {
  void* pointer = nullptr;
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    pointer = detail::system_impl::heap_allocate(heap_.context, bytes, alignment);
    if constexpr (Zero) {
      if (pointer != nullptr) {
        std::memset(pointer, 0, bytes);
      }
    }
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    if constexpr (Zero) {
      pointer = detail::mimalloc_impl::allocate_zeroed(heap_.context, bytes, alignment);
    } else {
      pointer = detail::mimalloc_impl::allocate(heap_.context, bytes, alignment);
    }
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    if constexpr (Zero) {
      pointer = detail::jemalloc_impl::allocate_zeroed(heap_.context, bytes, alignment);
    } else {
      pointer = detail::jemalloc_impl::allocate(heap_.context, bytes, alignment);
    }
  }
#endif
  return pointer;
}

UNIMEMORY_FORCE_INLINE void* Heap::reallocate_raw(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) noexcept {
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    void* next = allocate_raw<false>(new_bytes, alignment);
    if (next != nullptr) {
      std::memcpy(next, pointer, (std::min)(old_bytes, new_bytes));
      deallocate_raw(pointer, old_bytes, alignment);
    }
    return next;
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    return detail::mimalloc_impl::reallocate(heap_.context, pointer, old_bytes, new_bytes, alignment);
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    return detail::jemalloc_impl::reallocate(heap_.context, pointer, old_bytes, new_bytes, alignment);
  }
#endif
  else {
    return nullptr;
  }
}

UNIMEMORY_FORCE_INLINE void Heap::deallocate_raw(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    detail::system_impl::heap_deallocate(heap_.context, pointer, bytes, alignment);
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    detail::mimalloc_impl::deallocate(heap_.context, pointer, bytes, alignment);
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    detail::jemalloc_impl::deallocate(heap_.context, pointer, bytes, alignment);
  }
#endif
}

}  // namespace unimem
