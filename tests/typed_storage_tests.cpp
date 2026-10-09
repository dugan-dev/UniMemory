#include <unimem/memory.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* reason) {
    if (!condition) { throw std::runtime_error(reason); }
}
template<class Exception, class Function>
void throws(Function&& function) {
    try { function(); } catch (const Exception&) { return; }
    throw std::runtime_error("expected exception was not thrown");
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // The regression deliberately requests padded, over-aligned storage.
#endif
struct alignas(256) Record {
    static inline int constructed = 0;
    static inline int alive = 0;
    int value;
    std::string text;
    Record() = delete;
    explicit Record(int initial) : value(initial), text(std::to_string(initial)) {
        ++constructed;
        ++alive;
    }
    ~Record() noexcept { --alive; }
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

struct Throwing {
    static inline int attempts = 0;
    static inline int destroyed = 0;
    static inline std::array<int, 2> destruction_order{};
    int index = ++attempts;
    Throwing() {
        if (index == 3) { throw std::runtime_error("third constructor"); }
    }
    ~Throwing() noexcept { destruction_order[destroyed++] = index; }
};

void array_storage(unimem::Memory& memory, bool adapter) {
    const auto before = memory.statistics();
    const int constructed = Record::constructed;
    auto allocator = memory.allocator<Record>();
    auto* values = adapter ? allocator.allocate(4) : memory.allocate_objects<Record>(4);
    check(Record::constructed == constructed && Record::alive == 0,
          "typed allocation constructed elements");
    check(reinterpret_cast<std::uintptr_t>(values) % 256 == 0,
          "typed allocation lost over-alignment");
    if (before) {
        const auto after = *memory.statistics();
        check(after.allocations == before->allocations + 1 &&
              after.live_bytes == before->live_bytes + 4 * sizeof(Record),
              "typed allocation performed an extra allocation or requested extra bytes");
    }
    for (int index = 0; index < 4; ++index) { std::construct_at(values + index, index + 17); }
    check(values + 4 - values == 4 && values[0].text == "17" && values[3].text == "20",
          "constructed array lost its element storage");
    std::destroy_n(values, 4);
    if (adapter) { allocator.deallocate(values, 4); }
    else { memory.deallocate_objects(values, 4); }
    check(Record::alive == 0, "typed objects leaked");
    if (before) {
        check(memory.statistics()->live_bytes == before->live_bytes,
              "typed allocation did not balance its requested bytes");
    }
}

void boundaries(unimem::Memory& memory) {
    check(memory.allocate_objects<Record>(0) == nullptr, "zero object count must return null");
    auto allocator = memory.allocator<Record>();
    auto* empty = allocator.allocate(0);
    check(empty != nullptr && reinterpret_cast<std::uintptr_t>(empty) % 256 == 0,
          "zero allocator count must retain a pairable aligned allocation");
    allocator.deallocate(empty, 0);
    constexpr auto overflow = std::numeric_limits<std::size_t>::max() / sizeof(Record) + 1;
    throws<std::length_error>([&] { (void)memory.allocate_objects<Record>(overflow); });
    throws<std::length_error>([&] { (void)allocator.allocate(overflow); });
    Throwing::attempts = Throwing::destroyed = 0;
    throws<std::runtime_error>([&] { (void)memory.create_array<Throwing>(5); });
    check(Throwing::attempts == 3 && Throwing::destroyed == 2 &&
          Throwing::destruction_order == std::array{2, 1},
          "throwing array construction failed to destroy exactly the built elements");
    if (memory.statistics()) {
        check(memory.statistics()->live_bytes == 0, "failed construction leaked storage");
    }
}

void reuse_stack() {
    alignas(256) std::array<std::byte, 4096> buffer;
    auto memory = unimem::Memory::stack(buffer);
    for (int iteration = 0; iteration < 64; ++iteration) {
        // Reuse the same address for arrays of incompatible types and bounds.
        auto* integers = memory.allocate_objects<int>(17);
        check(static_cast<void*>(integers) == buffer.data(), "stack did not reuse storage");
        for (int index = 0; index < 17; ++index) { integers[index] = iteration + index; }
        check(integers[16] == iteration + 16, "implicit-lifetime array lost values");
        std::destroy_n(integers, 17);
        memory.reset();
        const auto mark = memory.mark();
        array_storage(memory, iteration % 2 != 0);
        check(memory.used() == 4 * sizeof(Record), "typed allocation consumed extra stack bytes");
        memory.rewind(mark);
        auto* values = memory.create_array<const double>(11);
        check(values[10] == 0.0, "cv-qualified array was not initialized");
        memory.destroy_array(values, 11);
        memory.reset();
    }
    boundaries(memory);
    memory.reset();
    const auto used = memory.used();
    throws<std::bad_alloc>([&] { (void)memory.allocate_objects<Record>(17); });
    check(memory.used() == used, "failed typed allocation consumed stack space");
    throws<std::bad_alloc>([&] { (void)memory.allocator<Record>().allocate(17); });
    check(memory.used() == used, "failed allocator allocation consumed stack space");
}

void low_alignment() {
    alignas(256) std::array<std::byte, 1025> buffer;
    auto memory = unimem::Memory::stack(buffer.data() + 1, 1024);
    for (bool adapter : {false, true}) {
        auto allocator = memory.allocator<char>();
        char* values = adapter ? allocator.allocate(16) : memory.allocate_objects<char>(16);
        check(static_cast<void*>(values) == buffer.data() + 1 && memory.used() == 16,
              "typed storage imposed alignment or padding beyond its element type");
        for (int index = 0; index < 16; ++index) { values[index] = static_cast<char>(index); }
        check(values[15] == 15, "minimally aligned array lost values");
        std::destroy_n(values, 16);
        if (adapter) { allocator.deallocate(values, 16); }
        else { memory.deallocate_objects(values, 16); }
        memory.reset();
    }
}
}

int main() {
    try {
        reuse_stack();
        low_alignment();
        for (auto backend : {unimem::Backend::Standard, unimem::Backend::Mimalloc,
                             unimem::Backend::Jemalloc}) {
            if (!unimem::available(backend)) { continue; }
            unimem::Memory::configure_global(backend, unimem::StatisticsMode::Basic);
            auto& memory = unimem::Memory::global(backend);
            array_storage(memory, false);
            array_storage(memory, true);
            boundaries(memory);
            if (unimem::capabilities(backend).heap) {
                auto heap = unimem::Memory::heap(backend, unimem::StatisticsMode::Basic);
                array_storage(heap, false);
                array_storage(heap, true);
                boundaries(heap);
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
