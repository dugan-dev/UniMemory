#include "compiled-test-config.h"
#include <unimem/all.h>
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
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void exercise(Memory& memory) {
  for (const auto alignment : {std::size_t{1}, std::size_t{8}, std::size_t{16}, std::size_t{64}, std::size_t{256}, std::size_t{4096}}) {
    for (const auto bytes : {std::size_t{1}, std::size_t{7}, std::size_t{100}, std::size_t{4096}}) {
      auto block = memory.make_block(bytes, alignment);
      check(reinterpret_cast<std::uintptr_t>(block.data()) % alignment == 0, "alignment guarantee");
      std::fill_n(static_cast<unsigned char*>(block.data()), bytes, 37);
      block.resize(bytes * 2);
      check(std::all_of(static_cast<unsigned char*>(block.data()), static_cast<unsigned char*>(block.data()) + bytes,
                        [](unsigned char value) { return value == 37; }),
            "reallocation lost data");
      block.resize(0);

      void* zeroed = memory.allocate_zeroed(bytes, alignment);
      check(std::all_of(static_cast<unsigned char*>(zeroed), static_cast<unsigned char*>(zeroed) + bytes,
                        [](unsigned char value) { return value == 0; }),
            "allocation was not zeroed");
      zeroed = memory.reallocate_zeroed(zeroed, bytes, bytes * 2, alignment);
      check(std::all_of(static_cast<unsigned char*>(zeroed), static_cast<unsigned char*>(zeroed) + bytes * 2,
                        [](unsigned char value) { return value == 0; }),
            "growth was not zeroed");
      memory.deallocate(zeroed, bytes * 2, alignment);
    }
  }
}

void defaults() {
  for (auto mode : {compiled_test::global_mode}) {
    Memory& first = compiled_test::global();
    Memory& second = compiled_test::global();
    for (unsigned round = 0; round < 16; ++round) {
      exercise(first);
      exercise(second);
    }
    if (mode == StatisticsMode::Basic) {
      check((!compiled_test::tracking() || (first.statistics().live_bytes == 0)) &&
                (!compiled_test::tracking() || (second.statistics().live_bytes == 0)),
            "default allocations leaked");
    }
  }
}

void arenas() {
  for (auto mode : compiled_test::heap_modes()) {
    for (unsigned round = 0; round < 16; ++round) {
      Heap first = compiled_test::heap(mode);
      Heap second = compiled_test::heap(mode);
      auto a = first.make_block(32);
      auto b = second.make_block(32);
      check(first.owns(a.data()) && !first.owns(b.data()) && second.owns(b.data()) && !second.owns(a.data()), "independent allocation groups");
      exercise(first);
      exercise(second);
    }
  }
  Memory& ordinary = compiled_test::global();
  exercise(ordinary);
}

void counters() {
  Global first(compiled_test::global_mode);
  Global second(compiled_test::global_mode);
  check(&first != &second && first.resource() != second.resource(), "Global contexts must be independent");
  auto a = first.make_block(100);
  auto b = second.make_block(200);
  check(mi_heap_of(a.data()) == mi_heap_of(b.data()), "Global allocations must use native defaults");
  check(!compiled_test::tracking() || (first.statistics().live_bytes == 100 && second.statistics().live_bytes == 200),
        "Global contexts must keep independent counters");
  const auto before_native = first.statistics();
  void* external = mi_malloc(512);
  check(external != nullptr, "native allocation failed");
  mi_free(external);
  check(first.statistics().allocations == before_native.allocations, "native allocations must not enter request counters");
  a.resize(300);
  check(!compiled_test::tracking() || (first.statistics().live_bytes == 300 && second.statistics().live_bytes == 200),
        "Global reallocation changed another context's counters");
  a.resize(0);
  check(!compiled_test::tracking() || (first.statistics().live_bytes == 0 && second.statistics().live_bytes == 200),
        "Global release changed another context's counters");
  auto large = first.make_block(1024 * 1024);
  check(!compiled_test::tracking() || first.statistics().live_bytes == large.size(), "live large-block request count");
  large.resize(0);
  check(!compiled_test::tracking() || first.statistics().live_bytes == 0, "large allocation release request count");
}

void threads() {
  Memory& first = compiled_test::global();
  Memory& second = compiled_test::global();
  Heap arena = compiled_test::heap(compiled_test::heap_mode);
  auto anchor = arena.make_block(32);
  std::atomic<bool> failed{false};
  std::vector<std::thread> workers;
  for (unsigned worker = 0; worker < 4; ++worker) {
    workers.emplace_back([&] {
      try {
        for (unsigned round = 0; round < 32; ++round) {
          exercise(first);
          exercise(second);
          exercise(arena);
        }
      } catch (...) {
        failed.store(true);
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  check(!failed.load(), "cross-thread path or content failed");
  anchor.resize(0);
  for (auto* memory : std::array<Memory*, 3>{&first, &second, &arena}) {
    const auto stats = memory->statistics();
    check((!compiled_test::tracking() || (stats.live_bytes == 0)) && (!compiled_test::tracking() || (stats.allocations == stats.deallocations)),
          "concurrent request counters did not balance");
  }
}
}  // namespace

int main(int argc, char** argv) {
  compiled_test::test_configuration(unimem::detail::Backend::Mimalloc, compiled_test::global_mode);
  try {
    const std::string_view mode = argc == 2 ? argv[1] : "";
    if (mode == "default") {
      defaults();
    } else if (mode == "arena") {
      arenas();
    } else if (mode == "counters") {
      counters();
    } else if (mode == "threads") {
      threads();
    } else {
      throw std::invalid_argument("unknown path test");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
