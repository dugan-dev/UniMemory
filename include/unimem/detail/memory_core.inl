#pragma once

#include <unimem/detail/backend.h>
#include <unimem/detail/configuration.h>
#include <unimem/detail/backend_system.inl>
#ifdef UNIMEMORY_WITH_MIMALLOC
#include <unimem/detail/backend_mimalloc.inl>
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
#include <unimem/detail/backend_jemalloc.inl>
#endif

#include <algorithm>
#include <climits>
#include <cstring>
#include <limits>
#include <mutex>

namespace unimem {

namespace detail {

struct TrackingRecord {
  std::size_t bytes;
  bool counted;
};

struct TrackingContext {
  BackendHandle base;
  mutable std::mutex mutex;
  std::unordered_map<void*, TrackingRecord> records;
  Statistics counters;
  StatisticsMode mode = StatisticsMode::Basic;
  TrackingContext(BackendHandle value) : base(value) {}
};

}  // namespace detail

namespace memory_impl {

UNIMEMORY_FORCE_INLINE bool valid_alignment(std::size_t alignment) noexcept {
  return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

inline std::mutex& configuration_mutex() {
  alignas(std::mutex) static std::byte storage[sizeof(std::mutex)];
  static std::mutex* initialized = ::new (storage) std::mutex;
  (void)initialized;
  return *std::launder(reinterpret_cast<std::mutex*>(storage));
}

inline void validate_statistics(StatisticsMode mode) {
  if (mode != StatisticsMode::Disabled && mode != StatisticsMode::Basic) {
    throw std::invalid_argument("UniMemory: invalid statistics mode");
  }
  if constexpr (detail::build_statistics == StatisticsMode::Disabled) {
    if (mode == StatisticsMode::Basic) {
      throw std::invalid_argument("UniMemory: statistics support is disabled in this build");
    }
  }
}

inline void update_peak(detail::TrackingContext* tracked) noexcept {
  tracked->counters.peak_live_bytes = (std::max)(tracked->counters.peak_live_bytes, tracked->counters.live_bytes);
}

template <bool Zero>
inline void* tracked_allocate_impl(void* context, std::size_t bytes, std::size_t alignment) noexcept {
  auto* tracked = static_cast<detail::TrackingContext*>(context);
  try {
    std::lock_guard lock(tracked->mutex);
    const bool enabled = tracked->mode == StatisticsMode::Basic;
    if (enabled) {
      tracked->records.emplace(nullptr, detail::TrackingRecord{bytes, true});
    }
    void* pointer = nullptr;
    if constexpr (Zero) {
      pointer = tracked->base.ops->allocate_zeroed != nullptr ? tracked->base.ops->allocate_zeroed(tracked->base.context, bytes, alignment)
                                                              : tracked->base.ops->allocate(tracked->base.context, bytes, alignment);
      if (pointer != nullptr && tracked->base.ops->allocate_zeroed == nullptr) {
        std::memset(pointer, 0, bytes);
      }
    } else {
      pointer = tracked->base.ops->allocate(tracked->base.context, bytes, alignment);
    }
    if (!enabled) {
      return pointer;
    }
    if (pointer == nullptr) {
      tracked->records.erase(nullptr);
      return nullptr;
    }
    auto record = tracked->records.extract(nullptr);
    record.key() = pointer;
    tracked->records.insert(std::move(record));
    ++tracked->counters.allocations;
    tracked->counters.live_bytes += bytes;
    update_peak(tracked);
    return pointer;
  } catch (...) {
    return nullptr;
  }
}

inline void* tracked_reallocate(void* context, void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) noexcept {
  auto* tracked = static_cast<detail::TrackingContext*>(context);
  try {
    std::lock_guard lock(tracked->mutex);
    const bool enabled = tracked->mode == StatisticsMode::Basic;
    bool existing = tracked->records.find(pointer) != tracked->records.end();
    if (enabled && !existing) {
      tracked->records.emplace(nullptr, detail::TrackingRecord{0, false});
    }
    void* next = nullptr;
    if (tracked->base.ops->reallocate != nullptr) {
      next = tracked->base.ops->reallocate(tracked->base.context, pointer, old_bytes, new_bytes, alignment);
    } else {
      next = tracked->base.ops->allocate(tracked->base.context, new_bytes, alignment);
      if (next != nullptr) {
        std::memcpy(next, pointer, (std::min)(old_bytes, new_bytes));
        tracked->base.ops->deallocate(tracked->base.context, pointer, old_bytes, alignment);
      }
    }
    if (next == nullptr) {
      if (enabled && !existing) {
        tracked->records.erase(nullptr);
      }
      return nullptr;
    }
    if (enabled || existing) {
      auto record = tracked->records.extract(existing ? pointer : nullptr);
      if (record.mapped().counted) {
        tracked->counters.live_bytes -= record.mapped().bytes;
      }
      record.key() = next;
      if (enabled) {
        record.mapped() = {new_bytes, true};
        ++tracked->counters.reallocations;
      } else {
        record.mapped().bytes = (std::min)(record.mapped().bytes, new_bytes);
      }
      if (record.mapped().counted) {
        tracked->counters.live_bytes += record.mapped().bytes;
      }
      tracked->records.insert(std::move(record));
      if (enabled) {
        update_peak(tracked);
      }
    }
    return next;
  } catch (...) {
    return nullptr;
  }
}

inline void tracked_deallocate(void* context, void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
  auto* tracked = static_cast<detail::TrackingContext*>(context);
  std::lock_guard lock(tracked->mutex);
  const auto record = tracked->records.find(pointer);
  tracked->base.ops->deallocate(tracked->base.context, pointer, bytes, alignment);
  if (tracked->mode == StatisticsMode::Basic) {
    ++tracked->counters.deallocations;
  }
  if (record != tracked->records.end()) {
    if (record->second.counted) {
      tracked->counters.live_bytes -= record->second.bytes;
    }
    tracked->records.erase(record);
  }
}

}  // namespace memory_impl

inline void Memory::config(std::int64_t release_delay_ms) {
  std::lock_guard lock(memory_impl::configuration_mutex());
  if (release_delay_ms < -1 || release_delay_ms > static_cast<std::int64_t>((std::numeric_limits<long>::max)())) {
    throw std::invalid_argument("UniMemory: invalid release delay");
  }
  bool accepted = true;
  if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
#ifdef UNIMEMORY_WITH_MIMALLOC
    accepted = detail::set_mimalloc_release_delay(release_delay_ms);
#endif
  } else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
#ifdef UNIMEMORY_WITH_JEMALLOC
    accepted = detail::set_jemalloc_release_delay(release_delay_ms);
#endif
  }
  if (!accepted) {
    throw std::runtime_error("UniMemory: backend rejected release delay");
  }
}

namespace memory_impl {
inline detail::BackendHandle selected_backend() {
  if constexpr (detail::build_backend == detail::Backend::Standard) {
    return detail::system_backend(false);
  }
#ifdef UNIMEMORY_WITH_MIMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Mimalloc) {
    return detail::mimalloc_backend(false);
  }
#endif
#ifdef UNIMEMORY_WITH_JEMALLOC
  else if constexpr (detail::build_backend == detail::Backend::Jemalloc) {
    return detail::jemalloc_backend(false);
  }
#endif
  else {
    throw std::logic_error("UniMemory: invalid compiled backend");
  }
}
}  // namespace memory_impl

inline Memory::Memory(MemoryKind kind) noexcept : kind_(kind), resource_(*this) {}

inline void Memory::initialize(detail::BackendHandle handle, StatisticsMode statistics) {
  if (statistics == StatisticsMode::Basic) {
    try {
      tracking_ = new detail::TrackingContext{handle};
    } catch (...) {
      handle.ops->destroy(handle.context);
      throw;
    }
  }
  ops_ = handle.ops;
  context_ = handle.context;
}

inline Memory::~Memory() {
  delete tracking_;
  if (ops_ != nullptr) {
    ops_->destroy(context_);
  }
}

inline detail::BackendHandle Memory::native_backend() const noexcept {
  return {ops_, context_};
}

inline void Memory::clear_allocation_statistics() noexcept {
  if (tracking_ == nullptr) {
    return;
  }
  std::lock_guard lock(tracking_->mutex);
  tracking_->records.clear();
  tracking_->counters = {};
}

inline Statistics Memory::statistics() const noexcept {
  if (tracking_ == nullptr) {
    return {};
  }
  std::lock_guard lock(tracking_->mutex);
  return tracking_->mode == StatisticsMode::Basic ? tracking_->counters : Statistics{};
}

inline void Memory::set_statistics(StatisticsMode statistics) {
  memory_impl::validate_statistics(statistics);
  if (tracking_ == nullptr) {
    if (statistics == StatisticsMode::Disabled) {
      return;
    }
    tracking_ = new detail::TrackingContext{native_backend()};
  }
  std::lock_guard lock(tracking_->mutex);
  tracking_->mode = statistics;
}

inline void Memory::reset_statistics() noexcept {
  if (tracking_ == nullptr) {
    return;
  }
  std::lock_guard lock(tracking_->mutex);
  tracking_->counters = {};
  for (auto& record : tracking_->records) {
    record.second.counted = false;
  }
}

inline void Memory::rewind_statistics(const void* buffer, std::size_t used) noexcept {
  if (tracking_ == nullptr) {
    return;
  }
  std::lock_guard lock(tracking_->mutex);
  const auto end = reinterpret_cast<std::uintptr_t>(buffer) + used;
  for (auto record = tracking_->records.begin(); record != tracking_->records.end();) {
    const auto start = reinterpret_cast<std::uintptr_t>(record->first);
    if (start >= end) {
      if (record->second.counted) {
        tracking_->counters.live_bytes -= record->second.bytes;
      }
      record = tracking_->records.erase(record);
    } else {
      const auto retained = (std::min)(record->second.bytes, static_cast<std::size_t>(end - start));
      if (record->second.counted) {
        tracking_->counters.live_bytes -= record->second.bytes - retained;
      }
      record->second.bytes = retained;
      ++record;
    }
  }
}

}  // namespace unimem
