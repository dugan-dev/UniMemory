#pragma once

#include <unimem/detail/backend.h>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4068)
#endif
#ifndef JEMALLOC_NO_RENAME
#define JEMALLOC_NO_RENAME
#define UNIMEMORY_RESTORE_JEMALLOC_RENAME
#endif
#include <jemalloc/jemalloc.h>
#ifdef UNIMEMORY_RESTORE_JEMALLOC_RENAME
#undef JEMALLOC_NO_RENAME
#undef UNIMEMORY_RESTORE_JEMALLOC_RENAME
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace unimem::detail {
namespace jemalloc_impl {

inline std::mutex& control_mutex() {
  alignas(std::mutex) static std::byte storage[sizeof(std::mutex)];
  static std::mutex* initialized = ::new (storage) std::mutex;
  (void)initialized;
  return *std::launder(reinterpret_cast<std::mutex*>(storage));
}

struct ArenaContext {
  unsigned index;
};

inline bool initialize() noexcept {
  static std::once_flag initialized;
  try {
    std::call_once(initialized, [] {
      const char* version = nullptr;
      std::size_t length = sizeof(version);
      if (je_mallctl("version", &version, &length, nullptr, 0) != 0 || length != sizeof(version) || version == nullptr) {
        throw std::runtime_error("UniMemory: jemalloc initialization failed");
      }
    });
    return true;
  } catch (...) {
    return false;
  }
}

UNIMEMORY_FORCE_INLINE int arena_flags(void* context) noexcept {
  return MALLOCX_ARENA(static_cast<ArenaContext*>(context)->index) | MALLOCX_TCACHE_NONE;
}

UNIMEMORY_FORCE_INLINE int flags_for(void* context, std::size_t alignment) noexcept {
  return MALLOCX_ALIGN(alignment) | (context == nullptr ? 0 : arena_flags(context));
}

UNIMEMORY_FORCE_INLINE void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept {
  return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr : je_mallocx(bytes, flags_for(context, alignment));
}

UNIMEMORY_FORCE_INLINE void* allocate_zeroed(void* context, std::size_t bytes, std::size_t alignment) noexcept {
  return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr : je_mallocx(bytes, flags_for(context, alignment) | MALLOCX_ZERO);
}

UNIMEMORY_FORCE_INLINE void* reallocate(void* context, void* pointer, std::size_t, std::size_t bytes, std::size_t alignment) noexcept {
  return alignment > static_cast<std::size_t>(INT_MAX) ? nullptr : je_rallocx(pointer, bytes, flags_for(context, alignment));
}

UNIMEMORY_FORCE_INLINE void deallocate(void* context, void* pointer, std::size_t, std::size_t) noexcept {
  je_dallocx(pointer, context == nullptr ? 0 : MALLOCX_TCACHE_NONE);
}

inline void destroy(void*) noexcept {}

inline void destroy_arena(void* context) noexcept {
  std::lock_guard lock(control_mutex());
  auto* arena = static_cast<ArenaContext*>(context);
  char name[64];
  const auto count = std::snprintf(name, sizeof(name), "arena.%u.destroy", arena->index);
  if (count > 0 && static_cast<std::size_t>(count) < sizeof(name)) {
    (void)je_mallctl(name, nullptr, nullptr, nullptr, 0);
  }
  delete arena;
}

inline void arena_command(void* context, const char* command) {
  char name[64];
  const auto count = std::snprintf(name, sizeof(name), "arena.%u.%s", static_cast<ArenaContext*>(context)->index, command);
  if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name) || je_mallctl(name, nullptr, nullptr, nullptr, 0) != 0) {
    throw std::runtime_error("UniMemory: jemalloc arena command failed");
  }
}

inline void* reset(void* context) {
  arena_command(context, "reset");
  return context;
}

inline void collect(void* context) {
  arena_command(context, "purge");
}

inline bool owns(void* context, const void* pointer) {
  unsigned index = 0;
  std::size_t length = sizeof(index);
  void* allocation = const_cast<void*>(pointer);
  if (je_mallctl("arenas.lookup", &index, &length, &allocation, sizeof(allocation)) != 0 || length != sizeof(index)) {
    throw std::runtime_error("UniMemory: jemalloc allocation lookup failed");
  }
  return index == static_cast<ArenaContext*>(context)->index;
}

inline void default_collect(void*) {
  ArenaContext all{MALLCTL_ARENAS_ALL};
  arena_command(&all, "purge");
}

inline const BackendOps ops{allocate, allocate_zeroed, reallocate, deallocate, destroy, nullptr, default_collect};
inline const BackendOps arena_ops{allocate, allocate_zeroed, reallocate, deallocate, destroy_arena, reset, collect, owns};

}  // namespace jemalloc_impl

inline BackendHandle jemalloc_backend(bool dedicated) {
  if (!jemalloc_impl::initialize()) {
    throw std::runtime_error("UniMemory: jemalloc initialization failed");
  }
  if (!dedicated) {
    return {&jemalloc_impl::ops, nullptr};
  }
  auto* arena = new jemalloc_impl::ArenaContext{};
  std::size_t length = sizeof(arena->index);
  {
    std::lock_guard lock(jemalloc_impl::control_mutex());
    if (je_mallctl("arenas.create", &arena->index, &length, nullptr, 0) != 0 || length != sizeof(arena->index)) {
      delete arena;
      throw std::bad_alloc();
    }
  }
  constexpr unsigned max_index = (1u << (sizeof(int) * CHAR_BIT - 21)) - 2u;
  if (arena->index > max_index) {
    jemalloc_impl::destroy_arena(arena);
    throw std::runtime_error("UniMemory: jemalloc arena index exceeds flag range");
  }
  return {&jemalloc_impl::arena_ops, arena};
}

inline bool set_jemalloc_release_delay(std::int64_t value) noexcept {
  if (!jemalloc_impl::initialize()) {
    return false;
  }
  using SignedSize = std::make_signed_t<std::size_t>;
  if (value < -1 || value > static_cast<std::int64_t>((std::numeric_limits<SignedSize>::max)())) {
    return false;
  }
  try {
    std::lock_guard lock(jemalloc_impl::control_mutex());
    auto next = static_cast<SignedSize>(value);
    const auto read = [](const char* name, auto& result) {
      std::size_t length = sizeof(result);
      return je_mallctl(name, &result, &length, nullptr, 0) == 0 && length == sizeof(result);
    };
    const auto write = [](const char* name, SignedSize setting) {
      return je_mallctl(name, nullptr, nullptr, &setting, sizeof(setting)) == 0;
    };
    struct ArenaPolicy {
      unsigned index;
      SignedSize dirty;
      SignedSize muzzy;
    };
    SignedSize old_dirty = 0;
    SignedSize old_muzzy = 0;
    if (!read("arenas.dirty_decay_ms", old_dirty) || !read("arenas.muzzy_decay_ms", old_muzzy)) {
      return false;
    }
    std::uint64_t epoch = 1;
    if (je_mallctl("epoch", nullptr, nullptr, &epoch, sizeof(epoch)) != 0) {
      return false;
    }
    unsigned count = 0;
    if (!read("arenas.narenas", count)) {
      return false;
    }
    std::vector<ArenaPolicy> policies;
    for (unsigned index = 0; index < count; ++index) {
      char name[64];
      bool initialized = false;
      std::snprintf(name, sizeof(name), "arena.%u.initialized", index);
      if (!read(name, initialized)) {
        return false;
      }
      if (!initialized) {
        continue;
      }
      ArenaPolicy policy{index, 0, 0};
      std::snprintf(name, sizeof(name), "arena.%u.dirty_decay_ms", index);
      if (!read(name, policy.dirty)) {
        return false;
      }
      std::snprintf(name, sizeof(name), "arena.%u.muzzy_decay_ms", index);
      if (!read(name, policy.muzzy)) {
        return false;
      }
      policies.push_back(policy);
    }
    const auto apply = [&](const ArenaPolicy& policy) {
      char name[64];
      std::snprintf(name, sizeof(name), "arena.%u.dirty_decay_ms", policy.index);
      const bool dirty = write(name, policy.dirty);
      std::snprintf(name, sizeof(name), "arena.%u.muzzy_decay_ms", policy.index);
      const bool muzzy = write(name, policy.muzzy);
      return dirty && muzzy;
    };
    const auto rollback = [&] {
      (void)write("arenas.dirty_decay_ms", old_dirty);
      (void)write("arenas.muzzy_decay_ms", old_muzzy);
      for (const auto& policy : policies) {
        (void)apply(policy);
      }
    };
    if (!write("arenas.dirty_decay_ms", next) || !write("arenas.muzzy_decay_ms", next)) {
      rollback();
      return false;
    }
    for (const auto& policy : policies) {
      if (!apply({policy.index, next, next})) {
        rollback();
        return false;
      }
    }
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace unimem::detail
