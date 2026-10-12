#include "memory_api_contract.h"
#include <array>
#include <memory>
#include <memory_resource>
#include <vector>

namespace {
struct Object {
  int value;
  Object(int initial = 0) : value(initial) {}
};

// Compile all common adapters through the shared non-virtual interface.
void common_operations(unimem::Memory& memory) {
  auto* raw = memory.allocate(16);
  raw = memory.reallocate(raw, 16, 32);
  raw = memory.reallocate_zeroed(raw, 32, 48);
  memory.deallocate(raw, 48);
  auto* zeroed = memory.allocate_zeroed(8);
  memory.deallocate(zeroed, 8);
  auto* storage = memory.allocate_objects<Object>(2);
  memory.deallocate_objects(storage, 2);
  auto unique = memory.make_unique<Object>(7);
  auto array = memory.make_unique_array<Object>(3);
  auto shared = memory.make_shared<Object>(11);
  auto adopted = memory.adopt_unique(memory.create<Object>(13));
  auto adopted_array = memory.adopt_unique_array(memory.create_array<Object>(2), 2);
  auto block = memory.make_block(16);
  block.resize(32);
  std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
  values.push_back(unique->value);
  std::pmr::vector<int> pmr_values(memory.resource());
  pmr_values.push_back(shared->value);
}
}  // namespace

void inheritance_compile_regression() {
  using namespace unimem;
  auto& global = Memory::global();
  Heap heap;
  std::array<std::byte, 4096> bytes{};
  Stack stack(bytes);
  for (Memory* memory : std::array<Memory*, 3>{&global, &heap, &stack}) {
    common_operations(*memory);
  }
  heap.collect();
  heap.reset();
  const auto mark = stack.mark();
  stack.allocate(16);
  stack.rewind(mark);
  stack.reset();
  std::unique_ptr<Heap> owned_heap = std::make_unique<Heap>();
  std::unique_ptr<Stack> owned_stack = std::make_unique<Stack>(bytes);
  common_operations(*owned_heap);
  common_operations(*owned_stack);
}
