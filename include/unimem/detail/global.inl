#pragma once

#include <atomic>

namespace unimem {

inline Global& Memory::global(StatisticsMode statistics) {
  memory_impl::validate_statistics(statistics);
  struct Slot {
    std::mutex mutex;
    std::atomic<Global*> value{nullptr};
    alignas(Global) std::byte object[sizeof(Global)];
  };
  alignas(Slot) static std::byte storage[sizeof(Slot)]{};
  static Slot* initialized = ::new (storage) Slot;
  (void)initialized;
  auto& slot = *std::launder(reinterpret_cast<Slot*>(storage));
  Global* global = slot.value.load(std::memory_order_acquire);
  if (global == nullptr) {
    std::lock_guard lock(slot.mutex);
    global = slot.value.load(std::memory_order_relaxed);
    if (global == nullptr) {
      global = ::new (slot.object) Global(statistics);
      slot.value.store(global, std::memory_order_release);
    }
  }
  return *global;
}

inline Global::Global(StatisticsMode statistics) : Memory(MemoryKind::Global) {
  memory_impl::validate_statistics(statistics);
  initialize(memory_impl::selected_backend(), statistics);
}

UNIMEMORY_FORCE_INLINE void* Global::allocate(std::size_t bytes, std::size_t alignment) {
  return allocate_for<MemoryKind::Global, false>(bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void* Global::allocate_zeroed(std::size_t bytes, std::size_t alignment) {
  return allocate_for<MemoryKind::Global, true>(bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void* Global::reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) {
  return reallocate_for<MemoryKind::Global>(pointer, old_bytes, new_bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void Global::deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
  deallocate_for<MemoryKind::Global>(pointer, bytes, alignment);
}

}  // namespace unimem
