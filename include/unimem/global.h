#pragma once

#include <unimem/memory.h>
#include <unimem/detail/members.h>

namespace unimem {

// Shared memory accessed through Memory::global().
class Global final : public Memory {
 public:
  // Raw storage in bytes.
  void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override;
  void* allocate_zeroed(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override;
  void* reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment = alignof(std::max_align_t)) override;
  void deallocate(void* pointer, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept override;

 private:
  UNIM_IMPL_GLOBAL_MEMBERS
};

}  // namespace unimem
