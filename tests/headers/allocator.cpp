#include <unimem/memory.h>

#include <list>
#include <memory_resource>
#include <vector>

namespace {
struct Point { int x; int y; };
}

bool header_allocator() {
    unimem::Memory& memory = unimem::Memory::global();
    unimem::Allocator<Point> allocator = memory.allocator<Point>();
    unimem::Allocator<long> rebound(allocator);
    Point* empty_storage = allocator.allocate(0);
    allocator.deallocate(empty_storage, 0);
    std::vector<Point, unimem::Allocator<Point>> points(allocator);
    std::list<Point, unimem::Allocator<Point>> nodes(allocator);
    std::pmr::vector<Point> temporary(memory.resource());
    for (int index = 0; index < 128; ++index) {
        points.push_back({index, index + 1});
        nodes.push_back(points.back());
        temporary.push_back(points.back());
    }
    return points.back().x == 127 && nodes.back().y == 128 &&
           temporary.back().y == 128 && rebound == allocator &&
           &rebound.memory() == &memory;
}
