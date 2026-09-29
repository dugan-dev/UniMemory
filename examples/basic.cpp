#include <unimem/memory.h>

#include <iostream>
#include <memory_resource>
#include <vector>

struct Point {
    float x;
    float y;
};

int main() {
    unimem::Memory& memory = unimem::Memory::global();
    Point* point = memory.create<Point>(1.0f, 2.0f);
    const Point position = *point;
    memory.destroy(point);

    unimem::OwnedBlock block = memory.make_block(1024, 64);
    std::pmr::vector<Point> points(memory.resource());
    points.push_back(position);
    std::cout << points.front().x + points.front().y
              << ", " << block.size() << " bytes\n";
    return points.front().x == 1.0f && points.front().y == 2.0f &&
           block.size() == 1024 ? 0 : 1;
}
