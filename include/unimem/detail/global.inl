#pragma once

#include <atomic>

namespace unimem {

namespace detail {
UNIMEMORY_FORCE_INLINE void* global_storage_address() noexcept {
  alignas(Global) static std::byte storage[sizeof(Global)]{};
  return storage;
}
UNIMEMORY_FORCE_INLINE std::atomic<bool>& global_initialized_flag() noexcept {
  static std::atomic<bool> initialized{false};
  return initialized;
}
inline std::mutex& global_initialization_mutex() {
  alignas(std::mutex) static std::byte storage[sizeof(std::mutex)];
  static std::mutex* initialized = ::new (storage) std::mutex;
  (void)initialized;
  return *std::launder(reinterpret_cast<std::mutex*>(storage));
}
}  // namespace detail

UNIMEMORY_NOINLINE inline void Memory::initialize_global(StatisticsMode statistics) {
  std::lock_guard lock(detail::global_initialization_mutex());
  auto& initialized = detail::global_initialized_flag();
  if (!initialized.load(std::memory_order_relaxed)) {
    ::new (detail::global_storage_address()) Global(statistics);
    initialized.store(true, std::memory_order_release);
  }
}

UNIMEMORY_FORCE_INLINE Global& Memory::global(StatisticsMode statistics) {
  memory_impl::validate_statistics(statistics);
  if (!detail::global_initialized_flag().load(std::memory_order_acquire)) {
    initialize_global(statistics);
  }
  return *std::launder(reinterpret_cast<Global*>(detail::global_storage_address()));
}

inline Global::Global(StatisticsMode statistics) : Memory(MemoryKind::Global) {
  memory_impl::validate_statistics(statistics);
  initialize(memory_impl::selected_backend(), statistics);
}

}  // namespace unimem
