#include <unimem/all.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

namespace {
using namespace unimem;
static_assert(detail::build_backend == detail::Backend::Jemalloc);

void check(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void allocate(Memory& memory, unsigned loops) {
  for (unsigned loop = 0; loop < loops; ++loop) {
    std::array<std::optional<OwnedBlock>, 64> blocks;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
      const auto bytes = 48 + ((i + loop) % 16) * 24;
      blocks[i].emplace(memory.make_block(bytes, 8));
      std::memset(blocks[i]->data(), 0x35, bytes);
    }
    for (const auto& block : blocks) {
      const auto* bytes = static_cast<const unsigned char*>(block->data());
      check(bytes[0] == 0x35 && bytes[block->size() - 1] == 0x35, "concurrent allocation data changed");
    }
  }
}

void release_policy() {
  Global global;
  auto existing = Heap();
  auto global_block = global.make_block(128);
  auto existing_block = existing.make_block(128);
  const auto check_policy = [](const void* pointer, std::int64_t expected) {
    unsigned arena = 0;
    std::size_t length = sizeof(arena);
    void* allocation = const_cast<void*>(pointer);
    check(je_mallctl("arenas.lookup", &arena, &length, &allocation, sizeof(allocation)) == 0, "policy arena lookup failed");
    for (const auto* option : {"dirty_decay_ms", "muzzy_decay_ms"}) {
      const auto name = "arena." + std::to_string(arena) + "." + option;
      std::make_signed_t<std::size_t> value = 0;
      length = sizeof(value);
      check(je_mallctl(name.c_str(), &value, &length, nullptr, 0) == 0 && length == sizeof(value), "policy read failed");
      check(value == expected, "release policy was not applied");
    }
  };
  for (const auto delay : {std::int64_t{-1}, std::int64_t{0}, std::int64_t{10}}) {
    Memory::config(delay);
    check_policy(global_block.data(), delay);
    check_policy(existing_block.data(), delay);
    auto future = Heap();
    auto future_block = future.make_block(128);
    check_policy(future_block.data(), delay);
  }
}

void worker(std::string_view mode, Memory* shared, unsigned index, unsigned loops) {
  if (mode == "mixed") {
    constexpr std::array modes{"global", "heap", "backend", "release_delay"};
    mode = modes[index % modes.size()];
  }
  if (mode == "shared") {
    allocate(*shared, loops);
  } else if (mode == "heap") {
    auto memory = Heap();
    allocate(memory, loops);
    auto block = memory.make_block(128, 64);
    check(memory.owns(block.data()), "heap allocation lost its arena");
  } else {
    if (mode == "backend") {
      check(unimem::detail::build_backend == detail::Backend::Jemalloc, "jemalloc backend identity");
    } else if (mode == "release_delay") {
      unimem::Memory::config(10);
    }
    Global memory;
    allocate(memory, loops);
  }
}
}  // namespace

int main(int argc, char** argv) {
  try {
    check(argc >= 2 && argc <= 4, "expected mode [rounds [loops]]");
    const std::string_view mode = argv[1];
    if (mode == "release_policy") {
      release_policy();
      return 0;
    }
    check(mode == "shared" || mode == "global" || mode == "heap" || mode == "backend" || mode == "release_delay" || mode == "mixed",
          "unknown initialization mode");
    const auto rounds = argc > 2 ? std::stoul(argv[2]) : 1;
    const auto loops = argc > 3 ? std::stoul(argv[3]) : 1;
    check(rounds >= 1 && rounds <= 64 && loops >= 1 && loops <= 64, "invalid workload size");
    // Only the shared case constructs Memory before starting its workers.
    // No jemalloc query or allocator warmup precedes the other cold entries.
    std::optional<Global> shared_context;
    if (mode == "shared") {
      shared_context.emplace();
    }
    Memory* shared = shared_context ? &*shared_context : nullptr;
    for (unsigned long round = 0; round < rounds; ++round) {
      std::array<std::exception_ptr, 6> failures;
      {
        std::array<std::thread, 6> workers;
        struct JoinWorkers {
          std::array<std::thread, 6>& workers;
          ~JoinWorkers() {
            for (auto& thread : workers) {
              if (thread.joinable()) {
                thread.join();
              }
            }
          }
        } join_workers{workers};
        for (unsigned index = 0; index < workers.size(); ++index) {
          workers[index] = std::thread([&, index] {
            try {
              worker(mode, shared, index, static_cast<unsigned>(loops));
            } catch (...) {
              failures[index] = std::current_exception();
            }
          });
        }
      }
      for (const auto& failure : failures) {
        if (failure) {
          std::rethrow_exception(failure);
        }
      }
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
