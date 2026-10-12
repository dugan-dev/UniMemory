#pragma once

#include <unimem/detail/members.h>

#include <cstddef>

namespace unimem {

class Memory;

// Owning byte storage with automatic release.
class OwnedBlock final {
 public:
  ~OwnedBlock();
  OwnedBlock(OwnedBlock&& other) noexcept;
  OwnedBlock& operator=(OwnedBlock&& other) noexcept;

  OwnedBlock(const OwnedBlock&) = delete;
  OwnedBlock& operator=(const OwnedBlock&) = delete;

  // Storage address, size and alignment.
  void* data() const noexcept;
  std::size_t size() const noexcept;
  std::size_t alignment() const noexcept;

  // Resize while preserving existing bytes.
  void resize(std::size_t new_bytes);

 private:
  UNIM_IMPL_OWNED_BLOCK_MEMBERS
};

}  // namespace unimem
