#include <unimem/memory.h>

namespace {
struct Point { int x; int y; };
}

bool header_smart_ptr() {
    unimem::Memory& memory = unimem::Memory::global();
    unimem::Unique<Point> unique = memory.make_unique<Point>(3, 4);
    unimem::UniqueArray<Point> array = memory.make_unique_array<Point>(3);
    std::shared_ptr<Point> shared = memory.make_shared<Point>(5, 6);
    std::weak_ptr<Point> weak = shared;
    Point* raw = unique.release();
    unique = memory.adopt_unique(raw);
    Point* raw_array = array.release();
    array = memory.adopt_unique_array(raw_array, 3);
    bool valid = unique->x == 3 && unique->y == 4 && array[2].x == 0 &&
                 array.get_deleter().count == 3 && shared->x == 5 && shared->y == 6;
    shared.reset();
    return valid && weak.expired();
}
