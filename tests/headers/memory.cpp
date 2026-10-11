#include <unimem/memory.h>
#include <unimem/version.h>
#include "standalone_types.h"

static_assert(UNIMEMORY_VERSION_MAJOR == 0);
static_assert(UNIMEMORY_VERSION_MINOR == 0);
static_assert(UNIMEMORY_VERSION_PATCH == 1);
static_assert(static_cast<int>(unimem::Memory::selected_backend) == UNIMEMORY_CONFIG_BACKEND);
static_assert(static_cast<int>(unimem::Memory::selected_statistics) == UNIMEMORY_CONFIG_STATISTICS);
static_assert(sizeof(void*) != 8 || sizeof(unimem::Memory) == 80);
static_assert(sizeof(void*) != 8 || sizeof(unimem::OwnedBlock) == 32);
static_assert(sizeof(unimem::Allocator<int>) == sizeof(void*));
static_assert(sizeof(unimem::Memory::Mark) == sizeof(void*) + sizeof(std::size_t) + sizeof(std::uint64_t));

constexpr bool selected_profile() {
    if constexpr (unimem::Memory::selected_backend == unimem::Backend::Standard) {
        return UNIMEMORY_CONFIG_BACKEND == 0;
    } else if constexpr (unimem::Memory::selected_backend == unimem::Backend::Mimalloc) {
        return UNIMEMORY_CONFIG_BACKEND == 1;
    } else {
        return UNIMEMORY_CONFIG_BACKEND == 2;
    }
}
static_assert(selected_profile());

namespace {
struct Point { int x; int y; };
}

bool header_memory() {
    unimem::Memory& memory = unimem::Memory::global();
    if (!standalone_allocator(memory)) { return false; }
    auto* object = memory.create<StandaloneObject>(17);
    auto* array = memory.create_array<StandaloneArrayElement>(3);
    array[2].value = 23;
    if (!standalone_smart_ptr(memory, object, array)) { return false; }
    Point* point = memory.create<Point>(3, 4);
    bool valid = point->x == 3 && point->y == 4;
    memory.destroy(point);
    Point* points = memory.create_array<Point>(3);
    valid = valid && points[2].x == 0 && points[2].y == 0;
    memory.destroy_array(points, 3);
    Point* storage = memory.allocate_objects<Point>();
    memory.deallocate_objects(storage);
    alignas(64) std::byte buffer[1024];
    unimem::Memory stack = unimem::Memory::stack(buffer);
    unimem::Memory::Mark mark = stack.mark();
    {
        unimem::Unique<Point> owner = stack.make_unique<Point>(5, 6);
        valid = valid && owner->x == 5 && owner->y == 6;
    }
    stack.rewind(mark);
    return valid && stack.used() == 0 && stack.capacity() == sizeof(buffer) &&
           stack.kind() == unimem::MemoryKind::Stack && !stack.backend();
}
