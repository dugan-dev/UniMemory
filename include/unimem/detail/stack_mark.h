#pragma once

#include <cstddef>
#include <cstdint>

namespace unimem {
class Stack;
}

namespace unimem::detail {

class StackMark final {
 public:
  StackMark(const StackMark&) = default;
  StackMark& operator=(const StackMark&) = default;

 private:
  friend class unimem::Stack;
  StackMark(const Stack* owner, std::size_t used, std::uint64_t generation) noexcept;

  const Stack* owner_;
  std::size_t used_;
  std::uint64_t generation_;
};

}  // namespace unimem::detail
