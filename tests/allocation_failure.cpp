#include "compiled-test-config.h"
#include <unimem/all.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <memory_resource>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif

namespace {
std::atomic<bool> fail_allocations{false};
// Make allocation results observable even when fault injection unexpectedly succeeds.
// Optimizers may otherwise remove an unused allocation/deallocation pair.
void* volatile observed_pointer = nullptr;
void* raw_allocate(std::size_t bytes, std::size_t alignment) {
  if (fail_allocations.load()) {
    throw std::bad_alloc();
  }
  bytes = bytes == 0 ? 1 : bytes;
  void* pointer = nullptr;
  if (alignment == 0) {
    pointer = std::malloc(bytes);
  } else {
#ifdef _WIN32
    pointer = _aligned_malloc(bytes, alignment);
#else
    if (posix_memalign(&pointer, alignment, bytes) != 0) {
      pointer = nullptr;
    }
#endif
  }
  if (!pointer) {
    throw std::bad_alloc();
  }
  return pointer;
}
void release_aligned(void* pointer) noexcept {
#ifdef _WIN32
  _aligned_free(pointer);
#else
  std::free(pointer);
#endif
}
template <class F>
bool fails(const char* stage, std::size_t alignment, unsigned iteration, F&& function) {
  fail_allocations.store(true);
  try {
    function();
  } catch (const std::bad_alloc&) {
    fail_allocations.store(false);
    return true;
  } catch (...) {
    fail_allocations.store(false);
    std::cerr << "unexpected exception: " << stage << ", alignment=" << alignment << ", iteration=" << iteration << '\n';
    throw;
  }
  fail_allocations.store(false);
  std::cerr << "fault injection did not throw: " << stage << ", alignment=" << alignment << ", iteration=" << iteration << '\n';
  return false;
}
}  // namespace

// Isolated executable: the library's Standard backend sees real allocation failures.
void* operator new(std::size_t n) {
  return raw_allocate(n, 0);
}
void* operator new[](std::size_t n) {
  return raw_allocate(n, 0);
}
void* operator new(std::size_t n, std::align_val_t a) {
  return raw_allocate(n, std::size_t(a));
}
void* operator new[](std::size_t n, std::align_val_t a) {
  return raw_allocate(n, std::size_t(a));
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  try {
    return raw_allocate(n, 0);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
  try {
    return raw_allocate(n, 0);
  } catch (...) {
    return nullptr;
  }
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  try {
    return raw_allocate(n, std::size_t(a));
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  try {
    return raw_allocate(n, std::size_t(a));
  } catch (...) {
    return nullptr;
  }
}
void operator delete(void* p) noexcept {
  std::free(p);
}
void operator delete[](void* p) noexcept {
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
  std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
  std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
  release_aligned(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
  release_aligned(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
  release_aligned(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
  release_aligned(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
  std::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
  std::free(p);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
  release_aligned(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
  release_aligned(p);
}

int main(int argc, char** argv) {
  using namespace unimem;
  if (argc == 2 && std::strcmp(argv[1], "initialization_retry") == 0) {
    compiled_test::configure(compiled_test::global_mode);
    bool rejected = false;
    fail_allocations.store(true);
    try {
      (void)Memory::global(compiled_test::global_mode);
    } catch (const std::bad_alloc&) {
      rejected = true;
    }
    fail_allocations.store(false);
    if (!rejected) {
      return 7;
    }
    auto& recovered = Memory::global(compiled_test::global_mode);
    auto* resource = recovered.resource();
    void* pointer = resource->allocate(0, 16);
    resource->deallocate(pointer, 0, 16);
    const auto stats = recovered.statistics();
    if (!compiled_test::tracking() || stats.allocations != 1 || stats.deallocations != 1 || stats.reallocations != 0 || stats.live_bytes != 0 ||
        stats.peak_live_bytes != 1) {
      return 8;
    }
    return 0;
  }
  compiled_test::test_configuration(detail::Backend::Standard, compiled_test::global_mode);
  Memory& memory = compiled_test::global();
  fail_allocations.store(true);
  {
    auto empty = memory.make_block(0);
    if (empty.data() || empty.size() != 0) {
      return 5;
    }
    alignas(std::max_align_t) std::byte buffer[1024];
    Stack stack = Memory::stack(buffer);
    auto object = stack.make_unique<int>(42);
    auto shared = stack.make_shared<int>(17);
    std::pmr::vector<int> values(stack.resource());
    values.assign(16, 9);
    if (*object != 42 || *shared != 17 || values.back() != 9) {
      return 6;
    }
  }
  fail_allocations.store(false);
  {
    for (auto alignment : {std::size_t{1}, std::size_t{16}, std::size_t{4096}}) {
      auto block = memory.make_block(33, alignment);
      void* original = block.data();
      std::memset(original, 0x75, 33);
      const auto release = [&](void* pointer) {
        memory.deallocate(pointer, alignment);
      };
      std::unique_ptr<void, decltype(release)> zeroed(memory.allocate(33, alignment), release);
      void* zeroed_original = zeroed.get();
      std::memset(zeroed_original, 0x75, 33);
      for (unsigned i = 0; i < 100; ++i) {
        const auto before = memory.statistics();
        if (!fails("allocate", alignment, i,
                   [&] {
                     void* pointer = memory.allocate(31, alignment);
                     observed_pointer = pointer;
                     memory.deallocate(pointer, alignment);
                   }) ||
            !fails("allocate_zeroed", alignment, i,
                   [&] {
                     void* pointer = memory.allocate_zeroed(31, alignment);
                     observed_pointer = pointer;
                     memory.deallocate(pointer, alignment);
                   }) ||
            !fails("resource.allocate", alignment, i,
                   [&] {
                     auto* resource = memory.resource();
                     void* pointer = resource->allocate(31, alignment);
                     observed_pointer = pointer;
                     resource->deallocate(pointer, 31, alignment);
                   }) ||
            !fails("block.resize", alignment, i, [&] { block.resize(65); }) ||
            !fails("reallocate_zeroed", alignment, i,
                   [&] {
                     void* pointer = memory.reallocate_zeroed(zeroed.get(), 33, 65, alignment);
                     observed_pointer = pointer;
                     // Successful reallocation consumed the old allocation. Adopt the result
                     // so a failed injection cannot leave a dangling owner or leak storage.
                     (void)zeroed.release();
                     zeroed.reset(pointer);
                   }) ||
            !fails("make_shared", alignment, i, [&] {
              auto p = memory.make_shared<int>(42);
              observed_pointer = p.get();
            })) {
          return 2;
        }
        if (block.data() != original || block.size() != 33 || zeroed.get() != zeroed_original) {
          std::cerr << "failed allocation changed ownership: alignment=" << alignment << ", iteration=" << i << '\n';
          return 3;
        }
        for (unsigned j = 0; j < 33; ++j) {
          if (static_cast<unsigned char*>(original)[j] != 0x75 || static_cast<unsigned char*>(zeroed_original)[j] != 0x75) {
            std::cerr << "failed allocation changed contents: alignment=" << alignment << ", iteration=" << i << ", byte=" << j << '\n';
            return 4;
          }
        }
        const auto after = memory.statistics();
        if (compiled_test::tracking() &&
            (before.live_bytes != after.live_bytes || before.allocations != after.allocations || before.deallocations != after.deallocations ||
             before.reallocations != after.reallocations || before.peak_live_bytes != after.peak_live_bytes)) {
          std::cerr << "failed allocation changed statistics: alignment=" << alignment << ", iteration=" << i << '\n';
          return 5;
        }
      }
    }
  }
  return 0;
}
