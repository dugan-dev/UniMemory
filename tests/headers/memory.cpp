#include <unimem/memory.h>
#include <unimem/all.h>
#include <unimem/version.h>
#include "memory_api_contract.h"
#include "standalone_types.h"
#include <memory>

static_assert(UNIMEMORY_VERSION_MAJOR == 0);
static_assert(UNIMEMORY_VERSION_MINOR == 0);
static_assert(UNIMEMORY_VERSION_PATCH == 1);
static_assert(static_cast<int>(unimem::detail::build_backend) == UNIMEMORY_CONFIG_BACKEND);
static_assert(static_cast<int>(unimem::detail::build_statistics) == UNIMEMORY_CONFIG_STATISTICS);
static_assert(sizeof(void*) != 8 || sizeof(unimem::OwnedBlock) == 32);
static_assert(sizeof(unimem::Allocator<int>) == sizeof(void*));
static_assert(sizeof(unimem::Stack::Mark) == sizeof(void*) + sizeof(std::size_t) + sizeof(std::uint64_t));

constexpr bool selected_profile() {
  if constexpr (unimem::detail::build_backend == unimem::detail::Backend::Standard) {
    return UNIMEMORY_CONFIG_BACKEND == 0;
  } else if constexpr (unimem::detail::build_backend == unimem::detail::Backend::Mimalloc) {
    return UNIMEMORY_CONFIG_BACKEND == 1;
  } else {
    return UNIMEMORY_CONFIG_BACKEND == 2;
  }
}
static_assert(selected_profile());

namespace {
struct Point {
  int x;
  int y;
};
struct StorageDeleter {
  unimem::Memory* memory;
  void operator()(Point* pointer) const noexcept { memory->deallocate_objects(pointer); }
};
}  // namespace

bool header_memory() {
  unimem::Global& memory = unimem::Memory::global();
  if (!standalone_allocator(memory)) {
    return false;
  }
  unimem::Unique<StandaloneObject> object(memory.create<StandaloneObject>(17), {&memory});
  unimem::UniqueArray<StandaloneArrayElement> array(memory.create_array<StandaloneArrayElement>(3), {&memory, 3});
  array[2].value = 23;
  if (!standalone_smart_ptr(memory, object.release(), array.release())) {
    return false;
  }
  unimem::Unique<Point> point(memory.create<Point>(3, 4), {&memory});
  bool valid = point->x == 3 && point->y == 4;
  point.reset();
  unimem::UniqueArray<Point> points(memory.create_array<Point>(3), {&memory, 3});
  valid = valid && points[2].x == 0 && points[2].y == 0;
  points.reset();
  std::unique_ptr<Point, StorageDeleter> storage(memory.allocate_objects<Point>(), StorageDeleter{&memory});
  storage.reset();
  alignas(64) std::byte buffer[1024];
  unimem::Stack stack = unimem::Memory::stack(buffer);
  unimem::Stack::Mark mark = stack.mark();
  {
    unimem::Unique<Point> owner = stack.make_unique<Point>(5, 6);
    valid = valid && owner->x == 5 && owner->y == 6;
  }
  stack.rewind(mark);
  return valid && stack.used() == 0 && stack.capacity() == sizeof(buffer) && stack.kind() == unimem::MemoryKind::Stack;
}
