#pragma once

#include <unimem/memory.h>
#include <unimem/detail/members.h>

namespace unimem {

// Independent allocation lifetime.
class Heap final : public Memory {
 public:
  Heap(StatisticsMode statistics = StatisticsMode::Disabled);
  ~Heap();

  // Reset does not invoke object destructors.
  void reset();

  // Collect unused storage and query allocation ownership.
  void collect();
  bool owns(const void* pointer) const;

 private:
  UNIM_IMPL_HEAP_MEMBERS
};

}  // namespace unimem
