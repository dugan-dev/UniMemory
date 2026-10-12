#include <unimem/allocator.h>
#include <unimem/all.h>

namespace {
struct Value {
  int value;
};
}  // namespace

bool standalone_allocator(unimem::Memory& memory) {
  unimem::Allocator<Value> allocator(memory);
  unimem::Allocator<long> rebound(allocator);
  return &allocator.memory() == &memory && &rebound.memory() == &memory && rebound == allocator && !(rebound != allocator);
}
