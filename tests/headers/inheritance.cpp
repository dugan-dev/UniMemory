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

struct RawDeleter {
  unimem::Memory* memory;
  std::size_t bytes;
  void operator()(void* pointer) const noexcept { memory->deallocate(pointer, bytes); }
};
using RawOwner = std::unique_ptr<void, RawDeleter>;

struct StorageDeleter {
  unimem::Memory* memory;
  void operator()(Object* pointer) const noexcept { memory->deallocate_objects(pointer, 2); }
};

// Successful reallocation has already released the old storage. Updating its
// owner must neither free that old address again nor throw after taking the new
// address; a failed reallocation leaves the original owner untouched.
void replace_reallocated(RawOwner& owner, void* pointer, std::size_t bytes) noexcept {
  (void)owner.release();
  owner.get_deleter().bytes = bytes;
  owner.reset(pointer);
}

// Compile all common adapters through the shared non-virtual interface.
void common_operations(unimem::Memory& memory) {
  RawOwner raw(memory.allocate(16), RawDeleter{&memory, 16});
  replace_reallocated(raw, memory.reallocate(raw.get(), 16, 32), 32);
  replace_reallocated(raw, memory.reallocate_zeroed(raw.get(), 32, 48), 48);
  raw.reset();
  RawOwner zeroed(memory.allocate_zeroed(8), RawDeleter{&memory, 8});
  zeroed.reset();
  std::unique_ptr<Object, StorageDeleter> storage(memory.allocate_objects<Object>(2), StorageDeleter{&memory});
  storage.reset();
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
