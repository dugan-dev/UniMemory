#include "compiled-test-config.h"
#include <unimem/all.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory_resource>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace unimem;

void check(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

detail::Backend backend_of(std::string_view name) {
  if (name == "standard") {
    return detail::Backend::Standard;
  }
  if (name == "mimalloc") {
    return detail::Backend::Mimalloc;
  }
  if (name == "jemalloc") {
    return detail::Backend::Jemalloc;
  }
  throw std::invalid_argument("unknown backend");
}

constexpr std::array<std::size_t, 6> sizes{1, 7, 64, 4096, 65536, 1048576};
constexpr std::array<std::size_t, 5> alignments{1, 8, 16, 256, 4096};

void reset_test() {
  for (auto mode : compiled_test::heap_modes()) {
    Heap arena = compiled_test::heap(mode);
    auto& memory = arena;
    auto* resource = memory.resource();
    for (unsigned round = 0; round < 64; ++round) {
      for (auto bytes : sizes) {
        for (auto alignment : alignments) {
          void* pointer = memory.allocate(bytes, alignment);
          check(arena.owns(pointer), "reset input ownership");
          std::memset(pointer, 0x35, bytes);
        }
      }
      // Raw allocations are discarded together; never query old pointers.
      arena.reset();
      check(resource == memory.resource(), "PMR adapter identity changed");
      check(mode == StatisticsMode::Basic || compiled_test::zero(memory.statistics()), "reset changed statistics mode");
      if (mode == StatisticsMode::Basic) {
        const auto stats = memory.statistics();
        check((!compiled_test::tracking() || (stats.allocations == 0)) && (!compiled_test::tracking() || (stats.deallocations == 0)) &&
                  (!compiled_test::tracking() || (stats.reallocations == 0)) && (!compiled_test::tracking() || (stats.live_bytes == 0)) &&
                  (!compiled_test::tracking() || (stats.peak_live_bytes == 0)),
              "reset did not clear counters");
      }
      arena.reset();
      auto block = memory.make_block(17, 256);
      check(arena.owns(block.data()), "allocation after reset");
    }
  }
}

void collect_test() {
  Heap arena = compiled_test::heap(compiled_test::heap_mode);
  arena.collect();
  for (unsigned round = 0; round < 64; ++round) {
    for (auto bytes : sizes) {
      auto live = arena.make_block(bytes, 256);
      std::memset(live.data(), 0x52, bytes);
      { auto freed = arena.make_block(bytes * 2); }
      const auto before = arena.statistics();
      arena.collect();
      arena.collect();
      check(arena.owns(live.data()), "collect changed ownership");
      for (std::size_t i = 0; i < bytes; ++i) {
        check(static_cast<unsigned char*>(live.data())[i] == 0x52, "collect corrupted live allocation");
      }
      const auto after = arena.statistics();
      check((!compiled_test::tracking() || (before.allocations == after.allocations)) &&
                (!compiled_test::tracking() || (before.deallocations == after.deallocations)) &&
                (!compiled_test::tracking() || (before.reallocations == after.reallocations)) &&
                (!compiled_test::tracking() || (before.live_bytes == after.live_bytes)) &&
                (!compiled_test::tracking() || (before.peak_live_bytes == after.peak_live_bytes)),
            "collect changed request counters");
    }
  }
  arena.reset();
  arena.collect();
}

void owns_test() {
  Heap first = Memory::heap();
  Heap second = Memory::heap();
  Memory& ordinary = compiled_test::global();
  check(!first.owns(nullptr), "null ownership");
  check(!first.owns(first.allocate(0)), "zero size ownership");
  for (unsigned round = 0; round < 32; ++round) {
    for (auto bytes : sizes) {
      for (auto alignment : alignments) {
        auto a = first.make_block(bytes, alignment);
        auto b = second.make_block(bytes, alignment);
        auto c = ordinary.make_block(bytes, alignment);
        check(first.owns(a.data()) && !second.owns(a.data()), "first ownership");
        check(second.owns(b.data()) && !first.owns(b.data()), "second ownership");
        check(!first.owns(c.data()) && !second.owns(c.data()), "default ownership");
        a.resize(bytes * 2);
        check(first.owns(a.data()) && !second.owns(a.data()), "reallocation ownership");
      }
    }
    first.reset();
    second.collect();
  }
}

void containers_test() {
  Heap arena = compiled_test::heap(compiled_test::heap_mode);
  for (unsigned round = 0; round < 128; ++round) {
    {
      auto& memory = arena;
      auto unique = memory.make_unique<int>(42);
      auto shared = memory.make_shared<int>(17);
      auto array = memory.make_unique_array<int>(64);
      std::vector<int, Allocator<int>> ordinary(memory.allocator<int>());
      std::pmr::vector<int> polymorphic(memory.resource());
      for (int i = 0; i < 512; ++i) {
        ordinary.push_back(i);
        polymorphic.push_back(i * 2);
      }
      arena.collect();
      check(*unique == 42 && *shared == 17 && ordinary.back() == 511 && polymorphic.back() == 1022, "owner content after collect");
      check(arena.owns(unique.get()) && arena.owns(array.get()) && arena.owns(ordinary.data()) && arena.owns(polymorphic.data()),
            "container ownership");
    }
    check((!compiled_test::tracking() || (arena.statistics().live_bytes == 0)), "owner release balance");
    arena.reset();
  }
}

void threads_test() {
  Heap arena = compiled_test::heap(compiled_test::heap_mode);
  for (unsigned epoch = 0; epoch < 8; ++epoch) {
    std::atomic<bool> failed{false};
    std::array<std::thread, 4> workers;
    for (auto& worker : workers) {
      worker = std::thread([&] {
        try {
          for (unsigned i = 0; i < 1000; ++i) {
            auto block = arena.make_block(1 + i % 4096, 64);
            check(arena.owns(block.data()), "thread ownership");
            std::memset(block.data(), 0x19, block.size());
            check(static_cast<unsigned char*>(block.data())[0] == 0x19, "thread allocation content");
          }
        } catch (...) {
          failed.store(true);
        }
      });
    }
    for (auto& worker : workers) {
      worker.join();
    }
    check(!failed.load(), "concurrent arena operation failed");
    check((!compiled_test::tracking() || (arena.statistics().live_bytes == 0)), "thread balance");
    // Control operations require exclusive access, after all workers join.
    {
      auto anchor = arena.make_block(4096, 64);
      std::memset(anchor.data(), 0x19, anchor.size());
      arena.collect();
      check(static_cast<unsigned char*>(anchor.data())[4095] == 0x19, "exclusive collection invalidated live storage");
    }
    arena.reset();
  }
}

struct DestructorProbe {
  unsigned* destroyed;
  ~DestructorProbe() noexcept { ++*destroyed; }
};

void lifetime_test() {
  unsigned destroyed = 0;
  for (unsigned round = 0; round < 128; ++round) {
    Heap arena = Memory::heap();
    auto* object = arena.create<DestructorProbe>(&destroyed);
    // No implicit C++ destructor is promised by bulk release.
    arena.reset();
    check(destroyed == round, "reset called an object destructor");
    object = arena.create<DestructorProbe>(&destroyed);
    arena.destroy(object);
    check(destroyed == round + 1, "explicit destructor missing");
    arena.allocate(65536, 256);
    // Arena destruction releases remaining raw allocations.
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 3) {
      throw std::invalid_argument("backend and mode required");
    }
    const auto backend = backend_of(argv[1]);
    compiled_test::test_configuration(backend, compiled_test::global_mode);
    const std::string_view mode = argv[2];
    if (mode == "reset") {
      reset_test();
    } else if (mode == "collect") {
      collect_test();
    } else if (mode == "owns") {
      owns_test();
    } else if (mode == "containers") {
      containers_test();
    } else if (mode == "threads") {
      threads_test();
    } else if (mode == "lifetime") {
      lifetime_test();
    } else {
      throw std::invalid_argument("unknown mode");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
