#pragma once

#include <cstddef>

namespace unimem {

class Memory;

// STL allocator bound to a Memory instance.
template <class T>
class Allocator {
 public:
  using value_type = T;

  Allocator(Memory& memory) noexcept;

  template <class U>
  Allocator(const Allocator<U>& other) noexcept;

  T* allocate(std::size_t count);
  void deallocate(T* pointer, std::size_t count) noexcept;

  Memory& memory() const noexcept;

  template <class U>
  bool operator==(const Allocator<U>& other) const noexcept;

  template <class U>
  bool operator!=(const Allocator<U>& other) const noexcept;

 private:
  Memory* memory_;
};

}  // namespace unimem
