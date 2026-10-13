// Native-only probe for a fixed jemalloc 5.3.1 SDK configured with --enable-tsan.
// clang++-18 -std=c++20 -g -O1 -fsanitize=thread -pthread -I<PREFIX>/include
//   tools/ci/jemalloc-tsan-probe.cpp -L<PREFIX>/lib -Wl,-rpath,<PREFIX>/lib -ljemalloc
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <thread>

namespace {
constexpr std::size_t worker_count = 8;
constexpr std::size_t batch_size = 256;
constexpr std::size_t iterations_per_wave = 5000;
constexpr std::array<std::size_t, 4> sizes{8, 64, 4096, 8192};

// The arrival release sequence publishes every participant's pointer/data
// writes before any participant consumes the preceding worker's batch.
class Barrier {
 public:
  void wait() noexcept {
    const auto generation = generation_.load(std::memory_order_acquire);
    if (arrived_.fetch_add(1, std::memory_order_acq_rel) + 1 == worker_count) {
      arrived_.store(0, std::memory_order_relaxed);
      generation_.fetch_add(1, std::memory_order_release);
    } else {
      while (generation_.load(std::memory_order_acquire) == generation) {
        std::this_thread::yield();
      }
    }
  }

 private:
  std::atomic<std::size_t> arrived_{0};
  std::atomic<std::size_t> generation_{0};
};

void refresh_epoch(std::atomic<std::size_t>& errors) noexcept {
  std::uint64_t epoch = 1;
  std::size_t length = sizeof(epoch);
  if (je_mallctl("epoch", &epoch, &length, &epoch, sizeof(epoch)) != 0 || length != sizeof(epoch)) {
    errors.fetch_add(1, std::memory_order_relaxed);
  }
}

bool run_wave(std::size_t wave, bool config, std::atomic<std::size_t>& errors) {
  std::array<std::array<void*, batch_size>, worker_count> batches{};
  std::array<std::thread, worker_count> workers;
  std::atomic<std::size_t> ready{0};
  std::atomic<bool> start{false}, cancelled{false};
  Barrier barrier;
  try {
    for (std::size_t worker = 0; worker < worker_count; ++worker) {
      workers[worker] = std::thread([&, worker] {
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        if (cancelled.load(std::memory_order_acquire)) {
          return;
        }
        for (std::size_t base = 0; base < iterations_per_wave; base += batch_size) {
          const auto count = (iterations_per_wave - base < batch_size) ? iterations_per_wave - base : batch_size;
          for (std::size_t slot = 0; slot < count; ++slot) {
            const auto identity = base + slot + worker + wave;
            const auto bytes = sizes[identity % sizes.size()];
            auto* pointer = je_mallocx(bytes, MALLOCX_ALIGN(16));
            batches[worker][slot] = pointer;
            if (pointer == nullptr) {
              errors.fetch_add(1, std::memory_order_relaxed);
            } else {
              std::memset(pointer, static_cast<unsigned char>(identity % 251 + 1), bytes);
            }
            if (config && slot % 64 == 0) {
              refresh_epoch(errors);
            }
          }
          barrier.wait();
          const auto previous = (worker + worker_count - 1) % worker_count;
          for (std::size_t slot = 0; slot < count; ++slot) {
            auto* pointer = static_cast<unsigned char*>(batches[previous][slot]);
            if (pointer != nullptr) {
              const auto identity = base + slot + previous + wave;
              const auto bytes = sizes[identity % sizes.size()];
              const auto expected = static_cast<unsigned char>(identity % 251 + 1);
              if (pointer[0] != expected || pointer[bytes - 1] != expected) {
                errors.fetch_add(1, std::memory_order_relaxed);
              }
              je_dallocx(pointer, 0);
            }
          }
          if (config) {
            refresh_epoch(errors);
          }
          // No producer may replace a slot until its remote consumer is done.
          barrier.wait();
        }
        // Returning destroys each worker's native allocator TLS/tcache state.
      });
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Thread creation failed: %s\n", error.what());
    cancelled.store(true, std::memory_order_release);
    start.store(true, std::memory_order_release);
    for (auto& worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    return false;
  }
  while (ready.load(std::memory_order_acquire) != worker_count) {
    std::this_thread::yield();
  }
  start.store(true, std::memory_order_release);
  for (auto& worker : workers) {
    worker.join();
  }
  if (config) {
    refresh_epoch(errors);
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 || (std::strcmp(argv[1], "handoff") != 0 && std::strcmp(argv[1], "config") != 0)) {
    std::fprintf(stderr, "Usage: jemalloc-tsan-probe handoff|config\n");
    return 2;
  }
  const bool config = std::strcmp(argv[1], "config") == 0;
  std::atomic<std::size_t> errors{0};
  for (std::size_t wave = 0; wave < 2; ++wave) {
    if (!run_wave(wave, config, errors)) {
      return 1;
    }
  }
  const auto failures = errors.load(std::memory_order_relaxed);
  std::printf("Native jemalloc %s: 8 workers, 10000 allocations/worker, 2 TLS lifetimes, %zu validation errors\n", argv[1], failures);
  return failures == 0 ? 0 : 1;
}
