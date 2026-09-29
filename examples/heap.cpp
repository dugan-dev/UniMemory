#include <unimem/memory.h>

#include <iostream>

struct Point {
    float x;
    float y;
};

int main() {
    for (unimem::Backend backend : {unimem::Backend::Mimalloc, unimem::Backend::Jemalloc}) {
        if (!unimem::capabilities(backend).heap) { continue; }
        unimem::Memory heap = unimem::Memory::heap(backend, unimem::StatisticsMode::Basic);
        {
            unimem::Unique<Point> point = heap.make_unique<Point>(1.0f, 2.0f);
            if (!heap.owns(point.get())) { return 1; }
            heap.collect();
            if (point->x != 1.0f || point->y != 2.0f) { return 2; }
        }
        heap.reset();
        if (heap.statistics()->live_bytes != 0) { return 3; }
        std::cout << "Heap reset succeeded\n";
    }
    return 0;
}
