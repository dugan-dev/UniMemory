#pragma once

#include <cstddef>
#include <memory>

namespace unimem {

class Memory;

// Destroy objects and release storage through their Memory.
template <class T>
struct Deleter {
  Memory* memory = nullptr;

  void operator()(T* pointer) const noexcept;
};

template <class T>
struct ArrayDeleter {
  Memory* memory = nullptr;
  std::size_t count = 0;

  void operator()(T* pointer) const noexcept;
};

// Exclusive ownership of objects or arrays.
template <class T>
using Unique = std::unique_ptr<T, Deleter<T>>;

template <class T>
using UniqueArray = std::unique_ptr<T[], ArrayDeleter<T>>;

}  // namespace unimem
