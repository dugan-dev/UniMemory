#pragma once

#include <unimem/memory.h>
#include <unimem/detail/members.h>

namespace unimem {

// Independent allocation lifetime.
class Heap final : public Memory {
 public:
  Heap(StatisticsMode statistics = StatisticsMode::Disabled);
  ~Heap() override;

  // Raw storage in bytes.
  void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override;
  void* allocate_zeroed(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override;
  void* reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment = alignof(std::max_align_t)) override;
  void deallocate(void* pointer, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept override;

  // Reset does not invoke object destructors.
  void reset();

  // Collect unused storage and query allocation ownership.
  void collect();
  bool owns(const void* pointer) const;

 private:
  UNIM_IMPL_HEAP_MEMBERS
};

}  // namespace unimem
