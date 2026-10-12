#pragma once

#include <unimem/memory.h>
#include <unimem/detail/stack_mark.h>
#include <unimem/detail/members.h>

namespace unimem {

// Fixed-capacity memory backed by a borrowed buffer.
class Stack final : public Memory {
 public:
  // Buffer storage must outlive this Stack and its users.
  Stack(void* buffer, std::size_t capacity, StatisticsMode statistics = StatisticsMode::Disabled);
  Stack(std::span<std::byte> buffer, StatisticsMode statistics = StatisticsMode::Disabled);
  ~Stack() = default;

  // Reset and rewind do not invoke object destructors.
  void reset();

  // Save and restore allocation checkpoints.
  using Mark = detail::StackMark;
  Mark mark() const;
  void rewind(Mark mark);

  // Buffer usage in bytes.
  std::size_t used() const noexcept;
  std::size_t capacity() const noexcept;

 private:
  UNIM_IMPL_STACK_MEMBERS
};

}  // namespace unimem
