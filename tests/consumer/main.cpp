#include <unimem/memory.h>
#include <unimem/version.h>

#include <memory_resource>
#include <vector>

static_assert(UNIMEMORY_VERSION_MAJOR == 0);
static_assert(UNIMEMORY_VERSION_MINOR == 0);
static_assert(UNIMEMORY_VERSION_PATCH == 1);

struct Point { float x; float y; };

bool header_common();
bool header_memory();
bool header_owned_block();
bool header_allocator();
bool header_smart_ptr();

int main() {
    if (!header_common() || !header_memory() || !header_owned_block() ||
        !header_allocator() || !header_smart_ptr()) { return 14; }
    unimem::Memory& memory = unimem::Memory::global();
    auto object = memory.make_unique<int>(42);
    if (*object != 42) { return 1; }
    unimem::Unique<Point> owner;
    owner = memory.adopt_unique(memory.create<Point>(1.0f, 2.0f));
    if (owner->x != 1.0f || owner->y != 2.0f) { return 10; }
    Point* raw = owner.release();
    owner = memory.adopt_unique(raw);
    unimem::UniqueArray<Point> points;
    points = memory.adopt_unique_array(memory.create_array<Point>(3), 3);
    if (points[2].x != 0.0f || points.get_deleter().count != 3) { return 11; }

    std::vector<int, unimem::Allocator<int>> ordinary(
        memory.allocator<int>());
    std::pmr::vector<int> polymorphic(memory.resource());
    ordinary.push_back(*object);
    polymorphic.push_back(ordinary.front());
    if (polymorphic.front() != 42) { return 2; }
    alignas(std::max_align_t) std::byte buffer[1024];
    unimem::Memory scratch = unimem::Memory::stack(buffer);
    {
        auto value = scratch.make_unique<int>(37);
        std::pmr::vector<int> items(scratch.resource());
        items.push_back(*value);
        if (items.front() != 37 || scratch.backend()) { return 8; }
    }
    scratch.reset();
    if (scratch.used() != 0 || !scratch.capabilities().checkpoints) { return 9; }

    const auto cap = unimem::capabilities(unimem::Backend::Standard);
    if (!cap.available || cap.heap || cap.detailed_statistics) { return 3; }
    for (auto backend : {unimem::Backend::Mimalloc, unimem::Backend::Jemalloc}) {
        if (!unimem::capabilities(backend).heap) { continue; }
        unimem::Memory arena = unimem::Memory::heap(backend, unimem::StatisticsMode::Basic);
        {
            auto value = arena.make_unique<int>(23);
            std::pmr::vector<int> items(arena.resource());
            items.push_back(*value);
            if (!arena.owns(value.get()) || !arena.owns(items.data())) { return 4; }
            arena.collect();
            if (*value != 23 || items.front() != 23) { return 5; }
        }
        arena.allocate(1024, 64);
        arena.reset();
        if (arena.statistics()->live_bytes != 0 || arena.owns(nullptr)) {
            return 6;
        }
        auto value = arena.make_unique<int>(29);
        if (!arena.owns(value.get()) || *value != 29) { return 7; }
    }
    return 0;
}
