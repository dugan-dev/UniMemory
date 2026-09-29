#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory_resource>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace unimem;
void check(bool value, const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
template<class E, class F> void throws(F&& function) {
    try { function(); } catch (const E&) { return; }
    throw std::runtime_error("expected exception missing");
}
Backend parse(const std::string& text) {
    if (text == "standard") { return Backend::Standard; }
    if (text == "mimalloc") { return Backend::Mimalloc; }
    if (text == "jemalloc") { return Backend::Jemalloc; }
    throw std::invalid_argument("unknown backend");
}

struct Slot {
    void* pointer = nullptr;
    std::size_t bytes = 0;
    std::size_t alignment = 1;
    unsigned char pattern = 0;
};
void verify(const Slot& slot) {
    if (!slot.pointer) { return; }
    const auto* begin = static_cast<const unsigned char*>(slot.pointer);
    check(std::all_of(begin, begin + slot.bytes,
        [&](unsigned char byte) { return byte == slot.pattern; }), "trace content mismatch");
    check(reinterpret_cast<std::uintptr_t>(slot.pointer) % slot.alignment == 0,
          "trace alignment mismatch");
}

void trace(Memory& memory, Memory* arena, unsigned seed, unsigned steps) {
    std::mt19937 random(seed);
    std::array<Slot, 128> slots{};
    std::uint64_t expected_live = 0;
    for (unsigned step = 0; step < steps; ++step) {
        auto& slot = slots[random() % slots.size()];
        verify(slot);
        if (slot.pointer && arena) { check(arena->owns(slot.pointer), "trace ownership"); }
        if (!slot.pointer) {
            slot.bytes = random() % 8193;
            slot.alignment = std::size_t{1} << (random() % 13);
            slot.pointer = step % 2 ? memory.allocate(slot.bytes, slot.alignment)
                                    : memory.allocate_zeroed(slot.bytes, slot.alignment);
            if (step % 2 == 0 && slot.pointer) {
                const auto* p = static_cast<const unsigned char*>(slot.pointer);
                check(std::all_of(p, p + slot.bytes, [](auto byte) { return byte == 0; }),
                      "trace zero allocation");
            }
            slot.pattern = static_cast<unsigned char>(random());
            if (slot.pointer) { std::memset(slot.pointer, slot.pattern, slot.bytes); }
            expected_live += slot.bytes;
        } else if (random() % 3 == 0) {
            memory.deallocate(slot.pointer, slot.bytes, slot.alignment);
            expected_live -= slot.bytes;
            slot = {};
        } else {
            const auto next_size = random() % 16385;
            const bool zeroed = step % 2 == 0;
            void* next = zeroed
                ? memory.reallocate_zeroed(slot.pointer, slot.bytes, next_size, slot.alignment)
                : memory.reallocate(slot.pointer, slot.bytes, next_size, slot.alignment);
            if (next) {
                const auto* p = static_cast<const unsigned char*>(next);
                check(std::all_of(p, p + std::min(slot.bytes, std::size_t{next_size}),
                    [&](auto byte) { return byte == slot.pattern; }), "trace realloc prefix");
                if (zeroed && next_size > slot.bytes) {
                    check(std::all_of(p + slot.bytes, p + next_size,
                        [](auto byte) { return byte == 0; }), "trace zero growth");
                }
                std::memset(next, slot.pattern, next_size);
            }
            expected_live = expected_live - slot.bytes + next_size;
            slot.pointer = next;
            slot.bytes = next_size;
        }
        if (const auto stats = memory.statistics()) {
            check(stats->live_bytes == expected_live && stats->peak_live_bytes >= expected_live,
                  "trace accounting mismatch");
        }
        if (arena && step % 521 == 0) { arena->collect(); }
    }
    for (auto& slot : slots) {
        verify(slot);
        memory.deallocate(slot.pointer, slot.bytes, slot.alignment);
    }
    if (const auto stats = memory.statistics()) {
        check(stats->live_bytes == 0 && stats->allocations == stats->deallocations,
              "trace cleanup mismatch");
    }
    if (arena) { arena->reset(); }
}

void random_test(Backend backend, unsigned seed, unsigned steps) {
    trace(Memory::global(backend), nullptr, seed, steps);
    for (auto mode : {StatisticsMode::Disabled, StatisticsMode::Basic}) {
        if (capabilities(backend).heap) {
            Memory arena = Memory::heap(backend, mode);
            trace(arena, &arena, seed, steps);
        }
    }
}

struct Object {
    static inline unsigned attempts = 0, alive = 0, fail_at = 0;
    Object() {
        if (++attempts == fail_at) { throw std::runtime_error("object failure"); }
        ++alive;
    }
    ~Object() noexcept { --alive; }
};
void objects(Backend backend) {
    Memory& memory = Memory::global(backend);
    for (unsigned fail = 1; fail <= 64; ++fail) {
        Object::attempts = Object::alive = 0;
        Object::fail_at = fail;
        throws<std::runtime_error>([&] { auto array = memory.make_unique_array<Object>(64); });
        check(Object::alive == 0 && memory.statistics()->live_bytes == 0,
              "partial construction leaked");
    }
    for (unsigned i = 0; i < 128; ++i) {
        Object::attempts = 0;
        Object::fail_at = 1;
        throws<std::runtime_error>([&] { auto p = memory.make_shared<Object>(); });
        check(memory.statistics()->live_bytes == 0, "shared constructor rollback");
        Object::fail_at = 0;
        auto shared = memory.make_shared<Object>();
        std::weak_ptr<Object> weak = shared;
        shared.reset();
        check(Object::alive == 0 && weak.expired() && memory.statistics()->live_bytes > 0,
              "weak control block lifetime");
        weak.reset();
        check(memory.statistics()->live_bytes == 0, "weak control block leaked");
    }
}

void containers(Backend backend) {
    Memory& first = Memory::global(backend);
    std::array<std::byte, 65536> buffer{};
    Memory second = [&]() -> Memory {
        if (capabilities(backend).heap) { return Memory::heap(backend, StatisticsMode::Basic); }
        return Memory::stack(buffer);
    }();
    for (unsigned round = 0; round < 64; ++round) {
        {
            std::vector<int, Allocator<int>> a(first.allocator<int>()), b(second.allocator<int>());
            for (int i = 0; i < 1024; ++i) { a.push_back(i); }
            b = a;
            check(&b.get_allocator().memory() == &second && b == a, "copy allocator propagation");
            std::vector<int, Allocator<int>> c(std::move(a), second.allocator<int>());
            check(c.size() == 1024 && c.back() == 1023, "unequal allocator move");
            a.assign(33, 19);
            b = std::move(a);
            check(b.size() == 33 && b.front() == 19 && &b.get_allocator().memory() == &second,
                  "move assignment allocator propagation");
            std::pmr::vector<std::pmr::string> nested(first.resource());
            for (unsigned i = 0; i < 64; ++i) { nested.emplace_back(128, 'x'); }
            auto copied = nested;
            check(copied.get_allocator().resource() == std::pmr::get_default_resource(),
                  "PMR copy selection contract");
            std::pmr::vector<std::pmr::string> rebound(nested, second.resource());
            check(rebound.front().get_allocator().resource() == second.resource(),
                  "nested PMR allocator propagation");
        }
        check(first.statistics()->live_bytes == 0 && (!second.statistics() || second.statistics()->live_bytes == 0),
              "container destruction balance");
        second.reset();
    }
}

void boundaries(Backend backend) {
    Memory& memory = Memory::global(backend);
    for (const auto alignment : {std::size_t{0}, std::size_t{3}, std::size_t{63},
                                 std::numeric_limits<std::size_t>::max()}) {
        throws<std::invalid_argument>([&] { memory.allocate(0, alignment); });
        throws<std::invalid_argument>([&] { memory.allocate_zeroed(19, alignment); });
        throws<std::invalid_argument>([&] { memory.make_block(19, alignment); });
        auto block = memory.make_block(31, 64);
        throws<std::invalid_argument>([&] { memory.reallocate(block.data(), 31, 0, alignment); });
        check(block.size() == 31, "invalid argument changed block");
    }
    throws<std::length_error>([&] { memory.create_array<std::uint64_t>(
        std::numeric_limits<std::size_t>::max()); });
    for (unsigned i = 0; i < 64; ++i) {
        auto block = memory.make_block(129, 256);
        void* original = block.data();
        std::memset(original, 0x67, 129);
        throws<std::bad_alloc>([&] { block.resize(std::numeric_limits<std::size_t>::max()); });
        check(block.data() == original && block.size() == 129, "failure lost ownership");
        auto moved = std::move(block);
        throws<std::logic_error>([&] { block.resize(0); });
        auto& same = moved;
        moved = std::move(same);
        check(moved.data() == original, "self move changed ownership");
    }
    throws<std::invalid_argument>([] { Memory& memory = Memory::global(static_cast<Backend>(99)); });
    throws<std::invalid_argument>([&] { Memory::configure_global(backend, static_cast<StatisticsMode>(99)); });
}

void handoff(Backend backend) {
    constexpr unsigned count = 4, batch = 64, rounds = 2048;
    std::array<Memory*, count> memories;
    for (auto& memory : memories) { memory = &Memory::global(backend); }
    std::array<std::array<void*, batch>, count> pointers{};
    std::barrier phase(count);
    std::atomic<bool> failed{false};
    std::array<std::thread, count> threads;
    for (unsigned id = 0; id < count; ++id) {
        threads[id] = std::thread([&, id] {
            for (unsigned round = 0; round < rounds; ++round) {
                for (auto& pointer : pointers[id]) {
                    try {
                        pointer = memories[id]->allocate(257, 256);
                        std::memset(pointer, 37, 257);
                    } catch (...) { failed.store(true); pointer = nullptr; }
                }
                phase.arrive_and_wait();
                const auto source = (id + 1) % count;
                for (auto pointer : pointers[source]) {
                    if (!pointer) { continue; }
                    const auto* bytes = static_cast<unsigned char*>(pointer);
                    if (bytes[0] != 37 || bytes[256] != 37) { failed.store(true); }
                    memories[source]->deallocate(pointer, 257, 256);
                }
                phase.arrive_and_wait();
            }
        });
    }
    for (auto& thread : threads) { thread.join(); }
    check(!failed.load(), "handoff content or allocation failed");
    for (auto& memory : memories) {
        const auto stats = *memory->statistics();
        check(stats.live_bytes == 0 && stats.allocations == count * rounds * batch &&
              stats.deallocations == stats.allocations, "handoff counters");
    }
}

void stack() {
    std::array<std::byte, 65536> backing{};
    for (unsigned offset = 0; offset < 64; ++offset) {
        Memory arena = Memory::stack(backing.data() + offset, backing.size() - offset);
        Memory other = Memory::stack(backing.data(), backing.size());
        for (unsigned alignment = 1; alignment <= 4096; alignment *= 2) {
            const auto mark = arena.mark();
            auto* pointer = arena.allocate(31, alignment);
            check(reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0, "scratch alignment");
            const auto used = arena.used();
            throws<std::bad_alloc>([&] { arena.allocate(std::numeric_limits<std::size_t>::max()); });
            check(arena.used() == used, "scratch failure changed offset");
            throws<std::invalid_argument>([&] { arena.rewind(other.mark()); });
            arena.rewind(mark);
            throws<std::invalid_argument>([&] { arena.rewind(mark); });
        }
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 3) { throw std::invalid_argument("backend and mode required"); }
        const auto backend = parse(argv[1]);
        const std::string mode = argv[2];
        Memory::configure_global(backend, StatisticsMode::Basic);
        if (mode == "trace") {
            if (argc != 4) { throw std::invalid_argument("trace seed required"); }
            random_test(backend, static_cast<unsigned>(std::stoul(argv[3])), 4096);
        }
        else if (mode == "soak") { random_test(backend, 0x024CAFE, 250000); }
        else if (mode == "objects") { objects(backend); }
        else if (mode == "containers") { containers(backend); }
        else if (mode == "boundaries") { boundaries(backend); }
        else if (mode == "handoff") { handoff(backend); }
        else if (mode == "stack") { stack(); }
        else { throw std::invalid_argument("unknown test mode"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "production test: " << error.what() << '\n';
        return 1;
    }
}
