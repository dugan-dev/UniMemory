#pragma once

#include <unimem/detail/configuration.h>

#include <cassert>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace unimem {

namespace detail {

struct TypedStorage {
  static UNIMEMORY_FORCE_INLINE void* operator new(std::size_t bytes, std::align_val_t alignment, Memory& memory) {
    return memory.allocate(bytes, static_cast<std::size_t>(alignment));
  }
};

template <class T>
UNIMEMORY_FORCE_INLINE T* allocate_array_storage(Memory& memory, std::size_t count) {
  static_assert(std::is_object_v<T> && !std::is_array_v<T>);
  if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(T)) {
    throw std::length_error("UniMemory: allocation size overflow");
  }
  if (count == 0) {
    return nullptr;
  }
  void* storage = TypedStorage::operator new(count * sizeof(T), std::align_val_t{alignof(T)}, memory);
  return *static_cast<T(*)[]>(storage);
}

}  // namespace detail

inline MemoryKind Memory::kind() const noexcept {
  return kind_;
}

template <class T>
UNIMEMORY_FORCE_INLINE T* Memory::allocate_objects(std::size_t count) {
  return detail::allocate_array_storage<T>(*this, count);
}

template <class T>
UNIMEMORY_FORCE_INLINE void Memory::deallocate_objects(T* pointer, std::size_t count) noexcept {
  deallocate(const_cast<std::remove_cv_t<T>*>(pointer), count * sizeof(T), alignof(T));
}

template <class T, class... Args>
UNIMEMORY_FORCE_INLINE T* Memory::create(Args&&... args) {
  static_assert(std::is_object_v<T> && !std::is_array_v<T>);
  static_assert(std::is_nothrow_destructible_v<T>);
  void* storage = allocate(sizeof(T), alignof(T));
  if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
    return ::new (storage) T(std::forward<Args>(args)...);
  } else {
    try {
      return ::new (storage) T(std::forward<Args>(args)...);
    } catch (...) {
      deallocate(storage, sizeof(T), alignof(T));
      throw;
    }
  }
}

template <class T>
UNIMEMORY_FORCE_INLINE void Memory::destroy(T* pointer) noexcept {
  static_assert(std::is_nothrow_destructible_v<T>);
  if (pointer != nullptr) {
    pointer->~T();
    deallocate(const_cast<std::remove_cv_t<T>*>(pointer), sizeof(T), alignof(T));
  }
}

template <class T>
inline void Memory::construct_array_with_rollback(T* objects, std::size_t count) {
  std::size_t built = 0;
  try {
    for (; built < count; ++built) {
      ::new (const_cast<std::remove_cv_t<T>*>(objects + built)) T();
    }
  } catch (...) {
    while (built != 0) {
      objects[--built].~T();
    }
    deallocate_objects(objects, count);
    throw;
  }
}

template <class T>
UNIMEMORY_FORCE_INLINE T* Memory::create_array(std::size_t count) {
  static_assert(std::is_object_v<T> && !std::is_array_v<T>);
  static_assert(std::is_nothrow_destructible_v<T>);
  T* objects = allocate_objects<T>(count);
  if constexpr (std::is_nothrow_default_constructible_v<T>) {
    for (std::size_t built = 0; built < count; ++built) {
      ::new (const_cast<std::remove_cv_t<T>*>(objects + built)) T();
    }
  } else {
    construct_array_with_rollback<T>(objects, count);
  }
  return objects;
}

template <class T>
UNIMEMORY_FORCE_INLINE void Memory::destroy_array(T* pointer, std::size_t count) noexcept {
  static_assert(std::is_nothrow_destructible_v<T>);
  if (pointer != nullptr) {
    const auto original_count = count;
    while (count != 0) {
      pointer[--count].~T();
    }
    deallocate_objects(pointer, original_count);
  }
}

template <class T, class... Args>
UNIMEMORY_FORCE_INLINE Unique<T> Memory::make_unique(Args&&... args) {
  return adopt_unique(create<T>(std::forward<Args>(args)...));
}

template <class T>
UNIMEMORY_FORCE_INLINE UniqueArray<T> Memory::make_unique_array(std::size_t count) {
  return adopt_unique_array(create_array<T>(count), count);
}

template <class T, class... Args>
UNIMEMORY_FORCE_INLINE std::shared_ptr<T> Memory::make_shared(Args&&... args) {
  static_assert(std::is_nothrow_destructible_v<std::remove_all_extents_t<T>>);
  return std::allocate_shared<T>(allocator<T>(), std::forward<Args>(args)...);
}

template <class T>
UNIMEMORY_FORCE_INLINE Unique<T> Memory::adopt_unique(T* pointer) noexcept {
  static_assert(std::is_object_v<T> && !std::is_array_v<T>);
  static_assert(std::is_nothrow_destructible_v<T>);
  return Unique<T>(pointer, Deleter<T>{this});
}

template <class T>
UNIMEMORY_FORCE_INLINE UniqueArray<T> Memory::adopt_unique_array(T* pointer, std::size_t count) noexcept {
  static_assert(std::is_object_v<T> && !std::is_array_v<T>);
  static_assert(std::is_nothrow_destructible_v<T>);
  return UniqueArray<T>(pointer, ArrayDeleter<T>{this, count});
}

}  // namespace unimem
