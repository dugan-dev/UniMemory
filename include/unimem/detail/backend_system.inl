#pragma once

#include <unimem/detail/backend.h>

#include <new>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace unimem::detail {
namespace system_impl {

inline constexpr std::size_t default_new_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

UNIMEMORY_FORCE_INLINE void* allocate(void*, std::size_t bytes, std::size_t alignment) noexcept {
  if (bytes > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)())) {
    return nullptr;
  }
  if (alignment > default_new_alignment) {
    return ::operator new(bytes, std::align_val_t(alignment), std::nothrow);
  }
  return ::operator new(bytes, std::nothrow);
}

UNIMEMORY_FORCE_INLINE void deallocate(void*, void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
  (void)bytes;
  if (alignment > default_new_alignment) {
    ::operator delete(pointer, std::align_val_t(alignment));
  } else {
    ::operator delete(pointer);
  }
}

inline void destroy(void*) noexcept {}
inline const BackendOps ops{allocate, nullptr, nullptr, deallocate, destroy};

struct Allocation {
  std::size_t bytes;
  std::size_t alignment;
};

struct HeapContext {
  BackendHandle base;
  HeapContext(BackendHandle value) : base(value) {}
  std::mutex mutex;
  std::unordered_map<void*, Allocation> allocations;
};

inline void* heap_allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept {
  auto* heap = static_cast<HeapContext*>(context);
  void* pointer = nullptr;
  try {
    std::lock_guard<std::mutex> lock(heap->mutex);
    pointer = heap->base.ops->allocate(heap->base.context, bytes, alignment);
    if (pointer == nullptr) {
      return nullptr;
    }
    heap->allocations.emplace(pointer, Allocation{bytes, alignment});
    return pointer;
  } catch (...) {
    if (pointer != nullptr) {
      heap->base.ops->deallocate(heap->base.context, pointer, bytes, alignment);
    }
    return nullptr;
  }
}

inline void heap_deallocate(void* context, void* pointer, std::size_t, std::size_t) noexcept {
  if (pointer == nullptr) {
    return;
  }
  auto* heap = static_cast<HeapContext*>(context);
  std::lock_guard<std::mutex> lock(heap->mutex);
  const auto allocation = heap->allocations.find(pointer);
  if (allocation == heap->allocations.end()) {
    return;
  }
  heap->base.ops->deallocate(heap->base.context, pointer, allocation->second.bytes, allocation->second.alignment);
  heap->allocations.erase(allocation);
}

inline void* heap_reset(void* context) {
  auto* heap = static_cast<HeapContext*>(context);
  std::lock_guard<std::mutex> lock(heap->mutex);
  for (const auto& allocation : heap->allocations) {
    heap->base.ops->deallocate(heap->base.context, allocation.first, allocation.second.bytes, allocation.second.alignment);
  }
  heap->allocations.clear();
  return context;
}

inline void heap_destroy(void* context) noexcept {
  heap_reset(context);
  delete static_cast<HeapContext*>(context);
}

inline void heap_collect(void* context) {
  auto* heap = static_cast<HeapContext*>(context);
  if (heap->base.ops->collect != nullptr) {
    heap->base.ops->collect(heap->base.context);
  }
}

inline bool heap_owns(void* context, const void* pointer) {
  if (pointer == nullptr) {
    return false;
  }
  auto* heap = static_cast<HeapContext*>(context);
  std::lock_guard<std::mutex> lock(heap->mutex);
  return heap->allocations.find(const_cast<void*>(pointer)) != heap->allocations.end();
}

inline const BackendOps heap_ops{heap_allocate, nullptr, nullptr, heap_deallocate, heap_destroy, heap_reset, heap_collect, heap_owns};

}  // namespace system_impl

inline BackendHandle system_backend(bool dedicated) {
  if (dedicated) {
    return {&system_impl::heap_ops, new system_impl::HeapContext({&system_impl::ops, nullptr})};
  }
  return {&system_impl::ops, nullptr};
}

}  // namespace unimem::detail
