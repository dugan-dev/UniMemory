#include <unimem/memory.h>
#include <unimem/version.h>

static_assert(UNIMEMORY_VERSION_MAJOR == 0);

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory_resource>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { std::abort(); } } while (false)

namespace {

struct alignas(256) OverAligned {
    int value = 17;
    std::byte padding[252]{};
};

struct Tracked {
    static inline int alive = 0;
    static inline int attempts = 0;
    static inline bool fail_third = false;
    Tracked() {
        if (fail_third && ++attempts == 3) { throw std::runtime_error("third"); }
        ++alive;
    }
    ~Tracked() noexcept { --alive; }
};

void test_backend(unimem::Backend backend) {
    unimem::Memory& memory = unimem::Memory::global(backend);
    {
        auto scalar = memory.make_unique<const int>(42);
        auto object = memory.make_unique<const OverAligned>();
        auto array = memory.make_unique_array<const OverAligned>(3);
        auto shared = memory.make_shared<const OverAligned>();
        CHECK(*scalar == 42 && object->value == 17 && array[2].value == 17);
        CHECK(shared->value == 17);
        const auto* storage = memory.allocate_objects<const OverAligned>();
        memory.deallocate_objects(storage);
        auto allocator = memory.allocator<const OverAligned>();
        const auto* allocated = allocator.allocate(2);
        allocator.deallocate(allocated, 2);
    }
    CHECK(memory.backend() == backend);
    CHECK(memory.allocate(0) == nullptr);
    memory.deallocate(nullptr, 0);
    CHECK(memory.allocate_objects<int>(0) == nullptr);
    auto* single_storage = memory.allocate_objects<OverAligned>();
    CHECK(reinterpret_cast<std::uintptr_t>(single_storage) % alignof(OverAligned) == 0);
    memory.deallocate_objects(single_storage);
    auto typed_allocator = memory.allocator<int>();
    int* zero_count = typed_allocator.allocate(0);
    CHECK(zero_count != nullptr);
    typed_allocator.deallocate(zero_count, 0);
    for (std::size_t alignment = 1; alignment <= 4096; alignment *= 2) {
        void* pointer = memory.allocate(37, alignment);
        CHECK(reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0);
        memory.deallocate(pointer, 37, alignment);
    }

    auto* zeroed = static_cast<std::byte*>(memory.allocate_zeroed(129, 256));
    CHECK(reinterpret_cast<std::uintptr_t>(zeroed) % 256 == 0);
    for (int i = 0; i < 129; ++i) { CHECK(zeroed[i] == std::byte{0}); }
    zeroed[0] = std::byte{42};
    zeroed = static_cast<std::byte*>(memory.reallocate(zeroed, 129, 300, 256));
    CHECK(reinterpret_cast<std::uintptr_t>(zeroed) % 256 == 0);
    CHECK(zeroed[0] == std::byte{42});
    CHECK(memory.reallocate(zeroed, 300, 0, 256) == nullptr);

    auto* growing = static_cast<unsigned char*>(memory.allocate(32, 64));
    for (int i = 0; i < 32; ++i) { growing[i] = 0xA5; }
    growing = static_cast<unsigned char*>(memory.reallocate_zeroed(growing, 32, 96, 64));
    for (int i = 0; i < 32; ++i) { CHECK(growing[i] == 0xA5); }
    for (int i = 32; i < 96; ++i) { CHECK(growing[i] == 0); }
    growing = static_cast<unsigned char*>(memory.reallocate_zeroed(growing, 96, 16, 64));
    for (int i = 0; i < 16; ++i) { CHECK(growing[i] == 0xA5); }
    memory.deallocate(growing, 16, 64);
    auto* from_null = static_cast<unsigned char*>(
        memory.reallocate_zeroed(nullptr, 123, 24, 64));
    for (int i = 0; i < 24; ++i) { CHECK(from_null[i] == 0); }
    memory.deallocate(from_null, 24, 64);

    {
        auto block = memory.make_block(64, 256);
        CHECK(block.size() == 64 && block.alignment() == 256);
        CHECK(reinterpret_cast<std::uintptr_t>(block.data()) % 256 == 0);
        static_cast<unsigned char*>(block.data())[0] = 91;
        block.resize(128);
        CHECK(static_cast<unsigned char*>(block.data())[0] == 91);
        auto moved = std::move(block);
        CHECK(block.data() == nullptr && moved.size() == 128);
        auto replaced = memory.make_block(8);
        replaced = std::move(moved);
        CHECK(moved.data() == nullptr && replaced.size() == 128);
        void* const original = replaced.data();
        bool failed = false;
        try { replaced.resize(std::numeric_limits<std::size_t>::max()); }
        catch (const std::bad_alloc&) {
            failed = true;
            CHECK(replaced.data() == original && replaced.size() == 128);
            CHECK(static_cast<unsigned char*>(replaced.data())[0] == 91);
        }
        CHECK(failed);
        replaced.resize(0);
        CHECK(replaced.data() == nullptr && replaced.size() == 0);
    }
    auto empty_block = memory.make_block(0);
    CHECK(empty_block.data() == nullptr && empty_block.size() == 0);

    auto* plain = memory.create<OverAligned>();
    CHECK(reinterpret_cast<std::uintptr_t>(plain) % 256 == 0);
    CHECK(plain->value == 17);
    memory.destroy(plain);

    auto* ints = memory.create_array<int>(12);
    for (int i = 0; i < 12; ++i) { CHECK(ints[i] == 0); }
    memory.destroy_array(ints, 12);
    CHECK(memory.create_array<int>(0) == nullptr);

    {
        auto owned = memory.make_unique<OverAligned>();
        auto array = memory.make_unique_array<int>(8);
        auto shared = memory.make_shared<std::string>("hello");
        CHECK(owned->value == 17 && array[0] == 0 && *shared == "hello");
        auto copy = shared;
        CHECK(copy.use_count() == 2);
    }

    {
        std::vector<int, unimem::Allocator<int>> ordinary(memory.allocator<int>());
        ordinary.push_back(9);
        std::list<int, unimem::Allocator<int>> ordinary_list(memory.allocator<int>());
        ordinary_list.push_back(10);
        using Pair = std::pair<const int, int>;
        std::map<int, int, std::less<int>, unimem::Allocator<Pair>> ordinary_map(
            std::less<int>{}, memory.allocator<Pair>());
        ordinary_map.emplace(7, 13);
        std::pmr::vector<std::pmr::string> nested(memory.resource());
        nested.emplace_back("a string longer than the small-string buffer");
        std::pmr::list<int> linked(memory.resource());
        linked.push_back(5);
        std::pmr::deque<int> deque(memory.resource());
        deque.push_back(8);
        std::pmr::map<int, int> ordered(memory.resource());
        ordered.emplace(2, 3);
        std::pmr::set<int> set(memory.resource());
        set.insert(11);
        std::pmr::unordered_map<int, int> hashed(memory.resource());
        hashed.emplace(4, 6);
        std::pmr::unordered_set<int> hash_set(memory.resource());
        hash_set.insert(12);
        CHECK(ordinary[0] == 9 && ordinary_list.front() == 10);
        CHECK(ordinary_map.at(7) == 13 && nested.size() == 1 && linked.front() == 5);
        CHECK(deque.front() == 8 && set.count(11) == 1);
        CHECK(ordered.at(2) == 3 && hashed.at(4) == 6 && hash_set.count(12) == 1);
        void* empty = memory.resource()->allocate(0, 1);
        CHECK(empty != nullptr);
        memory.resource()->deallocate(empty, 0, 1);
    }

    Tracked::attempts = 0;
    Tracked::fail_third = true;
    bool threw = false;
    try { (void)memory.create_array<Tracked>(5); }
    catch (const std::runtime_error&) { threw = true; }
    CHECK(threw && Tracked::alive == 0);
    Tracked::fail_third = false;
    {
        auto objects = memory.make_unique_array<Tracked>(4);
        CHECK(Tracked::alive == 4);
    }
    CHECK(Tracked::alive == 0);

    threw = false;
    try { (void)memory.allocate(16, 3); }
    catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { (void)memory.allocate_objects<int>(std::numeric_limits<std::size_t>::max()); }
    catch (const std::length_error&) { threw = true; }
    CHECK(threw);

    std::atomic<int> completed{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < 500; ++i) {
                auto owned = memory.make_unique<int>(i);
                CHECK(*owned == i);
            }
            ++completed;
        });
    }
    for (auto& worker : workers) { worker.join(); }
    CHECK(completed == 4);
}

void test_statistics(unimem::Backend backend) {

    unimem::Memory& tracked = unimem::Memory::global(backend);
    const auto baseline = *tracked.statistics();
    CHECK(baseline.live_bytes == 0);
    CHECK(tracked.allocate(0) == nullptr);
    tracked.deallocate(nullptr, 0);
    void* pointer = tracked.allocate(40);
    pointer = tracked.reallocate(pointer, 40, 80);
    auto failed_snapshot = *tracked.statistics();
    bool failed = false;
    try { (void)tracked.reallocate(pointer, 80,
                                   std::numeric_limits<std::size_t>::max()); }
    catch (const std::bad_alloc&) { failed = true; }
    CHECK(failed && tracked.statistics()->reallocations == failed_snapshot.reallocations);
    CHECK(tracked.statistics()->live_bytes == 80);
    tracked.deallocate(pointer, 80);
    auto snapshot = *tracked.statistics();
    CHECK(snapshot.allocations == baseline.allocations + 1 && snapshot.reallocations == baseline.reallocations + 1);
    CHECK(snapshot.deallocations == baseline.deallocations + 1 && snapshot.live_bytes == 0);
    CHECK(snapshot.peak_live_bytes >= 80);

    {
        auto block = tracked.make_block(25);
        block.resize(50);
        CHECK(tracked.statistics()->live_bytes == 50);
    }
    CHECK(tracked.statistics()->live_bytes == 0);

    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < 500; ++i) {
                auto owned = tracked.make_unique<int>(i);
                CHECK(*owned == i);
            }
        });
    }
    for (auto& worker : workers) { worker.join(); }
    snapshot = *tracked.statistics();
    CHECK(snapshot.allocations == snapshot.deallocations);
    CHECK(snapshot.live_bytes == 0 && snapshot.peak_live_bytes >= 80);
}

void test_stack_arena() {
    alignas(64) std::byte buffer[256]{};
    unimem::Memory arena = unimem::Memory::stack(buffer, sizeof(buffer));
    CHECK(arena.capacity() == sizeof(buffer));
    CHECK(arena.allocate(0) == nullptr && arena.used() == 0);
    auto start = arena.mark();
    void* first = arena.allocate(17, 64);
    CHECK(reinterpret_cast<std::uintptr_t>(first) % 64 == 0);
    auto middle = arena.mark();
    void* second = arena.allocate(23, 32);
    CHECK(reinterpret_cast<std::uintptr_t>(second) % 32 == 0);
    arena.rewind(middle);
    CHECK(arena.used() == 17);
    CHECK(arena.allocate(23, 32) == second);
    bool stale = false;
    try { arena.rewind(start); }
    catch (const std::invalid_argument&) { stale = true; }
    CHECK(stale);
    const auto before = arena.used();
    auto again = arena.mark();
    arena.rewind(again);
    CHECK(arena.used() == before);
    stale = false;
    try { arena.rewind(again); }
    catch (const std::invalid_argument&) { stale = true; }
    CHECK(stale);
    arena.reset();
    CHECK(arena.used() == 0);

    bool threw = false;
    try { (void)arena.allocate(257); }
    catch (const std::bad_alloc&) { threw = true; }
    CHECK(threw && arena.used() == 0);
    threw = false;
    try { (void)arena.allocate(1, 3); }
    catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    unimem::Memory other = unimem::Memory::stack(buffer, sizeof(buffer));
    threw = false;
    try { other.rewind(start); }
    catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    arena.reset();
    threw = false;
    try { arena.rewind(start); }
    catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

}

int main() {
    CHECK(unimem::available(unimem::Backend::Standard));
    for (auto backend : {unimem::Backend::Standard, unimem::Backend::Mimalloc, unimem::Backend::Jemalloc}) {
        if (unimem::available(backend)) {
            unimem::Memory::configure_global(backend, unimem::StatisticsMode::Basic);
        }
    }
    test_backend(unimem::Backend::Standard);
    test_statistics(unimem::Backend::Standard);
    test_stack_arena();
#ifdef UNIMEMORY_TEST_MIMALLOC
    CHECK(unimem::available(unimem::Backend::Mimalloc));
    test_backend(unimem::Backend::Mimalloc);
    test_statistics(unimem::Backend::Mimalloc);
#else
    CHECK(!unimem::available(unimem::Backend::Mimalloc));
#endif
#ifdef UNIMEMORY_TEST_JEMALLOC
    CHECK(unimem::available(unimem::Backend::Jemalloc));
    test_backend(unimem::Backend::Jemalloc);
    test_statistics(unimem::Backend::Jemalloc);
#else
    CHECK(!unimem::available(unimem::Backend::Jemalloc));
#endif
}
