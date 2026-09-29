#include <unimem/memory.h>
#include <unimem/version.h>

static_assert(UNIMEMORY_VERSION_MAJOR == 0);
static_assert(UNIMEMORY_VERSION_MINOR == 0);
static_assert(UNIMEMORY_VERSION_PATCH == 1);

namespace {
struct Point { int x; int y; };
}

bool header_memory() {
    unimem::Memory& memory = unimem::Memory::global();
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
