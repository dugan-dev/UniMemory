#pragma once

#include <unimem/common.h>
#include <unimem/detail/configuration.h>
#include <unimem/detail/members.h>
#include <unimem/allocator.h>
#include <unimem/owned_block.h>
#include <unimem/smart_ptr.h>

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <span>

namespace unimem {

class Global;
class Heap;
class Stack;

// Keep Memory alive until its owners, containers and weak pointers are gone.
class Memory {
 public:
  // Unused-page release policy.
  static void config(std::int64_t release_delay_ms);

  // Shared global memory and independent heap or stack memory.
  static Global& global(StatisticsMode statistics = StatisticsMode::Disabled);
  static Heap heap(StatisticsMode statistics = StatisticsMode::Disabled);
  static Stack stack(void* buffer, std::size_t capacity, StatisticsMode statistics = StatisticsMode::Disabled);
  static Stack stack(std::span<std::byte> buffer, StatisticsMode statistics = StatisticsMode::Disabled);

  virtual ~Memory();
  Memory(const Memory&) = delete;
  Memory& operator=(const Memory&) = delete;
  Memory(Memory&&) = delete;
  Memory& operator=(Memory&&) = delete;

  MemoryKind kind() const noexcept;

  // Raw storage in bytes.
  virtual void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) = 0;
  virtual void* allocate_zeroed(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) = 0;
  virtual void* reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment = alignof(std::max_align_t)) = 0;
  void* reallocate_zeroed(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment = alignof(std::max_align_t));
  virtual void deallocate(void* pointer, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept = 0;

  // Typed storage without construction or destruction.
  template <class T>
  T* allocate_objects(std::size_t count = 1);

  template <class T>
  void deallocate_objects(T* pointer, std::size_t count = 1) noexcept;

  // Construct and destroy objects or arrays.
  template <class T, class... Args>
  T* create(Args&&... args);

  template <class T>
  void destroy(T* pointer) noexcept;

  template <class T>
  T* create_array(std::size_t count);

  template <class T>
  void destroy_array(T* pointer, std::size_t count) noexcept;

  // Owning blocks and smart pointers.
  OwnedBlock make_block(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

  template <class T, class... Args>
  Unique<T> make_unique(Args&&... args);

  template <class T>
  UniqueArray<T> make_unique_array(std::size_t count);

  template <class T, class... Args>
  std::shared_ptr<T> make_shared(Args&&... args);

  template <class T>
  Unique<T> adopt_unique(T* pointer) noexcept;

  template <class T>
  UniqueArray<T> adopt_unique_array(T* pointer, std::size_t count) noexcept;

  // STL and PMR container adapters.
  template <class T>
  Allocator<T> allocator() noexcept;

  std::pmr::memory_resource* resource() noexcept;

  // Allocation counters and requested-byte usage.
  Statistics statistics() const noexcept;

  // Change or reset statistics only while this Memory is idle.
  void set_statistics(StatisticsMode statistics);
  void reset_statistics() noexcept;

 protected:
  UNIM_IMPL_MEMORY_MEMBERS
};

}  // namespace unimem
