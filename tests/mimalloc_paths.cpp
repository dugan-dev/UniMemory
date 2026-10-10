#include "compiled-test-config.h"
#include <unimem/memory.h>
#include <mimalloc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace unimem;

void check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void exercise(Memory& memory, mi_heap_t* heap) {
    for (const auto alignment : {std::size_t{1}, std::size_t{8}, std::size_t{16},
                                 std::size_t{64}, std::size_t{256}, std::size_t{4096}}) {
        for (const auto bytes : {std::size_t{1}, std::size_t{7}, std::size_t{100},
                                std::size_t{4096}}) {
            auto block = memory.make_block(bytes, alignment);
            check(mi_heap_of(block.data()) == heap, "wrong allocation heap");
            check(reinterpret_cast<std::uintptr_t>(block.data()) % alignment == 0,
                  "alignment guarantee");
            std::fill_n(static_cast<unsigned char*>(block.data()), bytes, 37);
            block.resize(bytes * 2);
            check(mi_heap_of(block.data()) == heap, "wrong reallocation heap");
            check(std::all_of(static_cast<unsigned char*>(block.data()),
                              static_cast<unsigned char*>(block.data()) + bytes,
                              [](unsigned char value) { return value == 37; }),
                  "reallocation lost data");
            block.resize(0);

            void* zeroed = memory.allocate_zeroed(bytes, alignment);
            check(mi_heap_of(zeroed) == heap, "wrong zeroed allocation heap");
            check(std::all_of(static_cast<unsigned char*>(zeroed),
                              static_cast<unsigned char*>(zeroed) + bytes,
                              [](unsigned char value) { return value == 0; }),
                  "allocation was not zeroed");
            zeroed = memory.reallocate_zeroed(zeroed, bytes, bytes * 2, alignment);
            check(mi_heap_of(zeroed) == heap, "wrong zeroed reallocation heap");
            check(std::all_of(static_cast<unsigned char*>(zeroed),
                              static_cast<unsigned char*>(zeroed) + bytes * 2,
                              [](unsigned char value) { return value == 0; }),
                  "growth was not zeroed");
            memory.deallocate(zeroed, bytes * 2, alignment);
        }
    }
}

void defaults() {
    for (auto mode : {compiled_test::global_mode}) {
        Memory& first = Memory::global(Backend::Mimalloc);
        Memory& second = Memory::global(Backend::Mimalloc);
        for (unsigned round = 0; round < 16; ++round) {
            exercise(first, mi_heap_main());
            exercise(second, mi_heap_main());
        }
        check( (!compiled_test::supports_statistics || (first.backend_statistics()->scope == BackendStatisticsScope::Process)) ,
              "default statistics must cover the backend");
        const auto native = *first.backend_statistics();
        check(!native.requested_bytes && !native.allocated_bytes &&
              native.committed_bytes && native.reserved_bytes,
              "unreliable native malloc counters must remain unavailable");
        if (mode == StatisticsMode::Basic) {
            check( (!compiled_test::supports_statistics || (first.statistics()->live_bytes == 0)) && (!compiled_test::supports_statistics || (second.statistics()->live_bytes == 0)) ,
                  "default allocations leaked");
        }
    }
}

void arenas() {
    for (auto mode : compiled_test::heap_modes()) {
        for (unsigned round = 0; round < 16; ++round) {
            Memory first = Memory::heap(Backend::Mimalloc, mode);
            Memory second = Memory::heap(Backend::Mimalloc, mode);
            auto a = first.make_block(32);
            auto b = second.make_block(32);
            auto* heap_a = mi_heap_of(a.data());
            auto* heap_b = mi_heap_of(b.data());
            check(heap_a != heap_b && heap_a != mi_heap_main() && heap_b != mi_heap_main(),
                  "arenas must have independent heaps");
            exercise(first, heap_a);
            exercise(second, heap_b);
            check(!first.capabilities().detailed_statistics && !first.backend_statistics() &&
                  !second.capabilities().detailed_statistics && !second.backend_statistics(),
                  "unsupported native Heap metrics must remain unavailable");
        }
    }
    Memory& ordinary = Memory::global(Backend::Mimalloc);
    exercise(ordinary, mi_heap_main());
}

void counters() {
    Memory& first = Memory::global(Backend::Mimalloc);
    Memory& second = Memory::global(Backend::Mimalloc);
    auto a = first.make_block(100);
    auto b = second.make_block(200);
    check(mi_heap_of(a.data()) == mi_heap_of(b.data()), "ordinary memories must share defaults");
    check( (!compiled_test::supports_statistics || (first.statistics()->live_bytes == 300)) && &first == &second,
          "global references must share request counters");
    void* external = mi_malloc(512);
    check(external != nullptr, "native allocation failed");
    mi_free(external);
    check( (!compiled_test::supports_statistics || (first.statistics()->allocations == 2)) && (!compiled_test::supports_statistics || (second.statistics()->allocations == 2)) ,
          "native allocations must not enter request counters");
    a.resize(300);
    check( (!compiled_test::supports_statistics || (first.statistics()->live_bytes == 500)) && (!compiled_test::supports_statistics || (second.statistics()->live_bytes == 500)) ,
          "reallocation changed another memory's counters");
    a.resize(0);
    check( (!compiled_test::supports_statistics || (first.statistics()->live_bytes == 200)) && (!compiled_test::supports_statistics || (second.statistics()->live_bytes == 200)) ,
          "release changed another memory's counters");
    auto large = first.make_block(1024 * 1024);
    check( (!compiled_test::supports_statistics || (first.statistics()->live_bytes == 200 + large.size())) , "live large-block request count");
    auto native = *first.backend_statistics();
    check(!native.requested_bytes && !native.allocated_bytes,
          "large allocation exposed unreliable native counters");
    large.resize(0);
    native = *first.backend_statistics();
    check( (!compiled_test::supports_statistics || (first.statistics()->live_bytes == 200)) && !native.requested_bytes && !native.allocated_bytes,
          "stale native huge counter must not be a live-byte metric");
}

void threads() {
    Memory& first = Memory::global(Backend::Mimalloc);
    Memory& second = Memory::global(Backend::Mimalloc);
    Memory arena = Memory::heap(Backend::Mimalloc, compiled_test::heap_mode);
    auto anchor = arena.make_block(32);
    auto* arena_heap = mi_heap_of(anchor.data());
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (unsigned worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&] {
            try {
                for (unsigned round = 0; round < 32; ++round) {
                    exercise(first, mi_heap_main());
                    exercise(second, mi_heap_main());
                    exercise(arena, arena_heap);
                }
            } catch (...) { failed.store(true); }
        });
    }
    for (auto& worker : workers) { worker.join(); }
    check(!failed.load(), "cross-thread path or content failed");
    anchor.resize(0);
    for (auto* memory : {&first, &second, &arena}) {
        const auto stats = memory->statistics();
        check( (!compiled_test::supports_statistics || (stats->live_bytes == 0)) && (!compiled_test::supports_statistics || (stats->allocations == stats->deallocations)) ,
              "concurrent request counters did not balance");
    }
}
}

int main(int argc, char** argv) {
    compiled_test::test_configuration(unimem::Backend::Mimalloc, compiled_test::global_mode);
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "";
        if (mode == "default") { defaults(); }
        else if (mode == "arena") { arenas(); }
        else if (mode == "counters") { counters(); }
        else if (mode == "threads") { threads(); }
        else { throw std::invalid_argument("unknown path test"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
