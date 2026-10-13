#include "compiled-test-config.h"
#include <unimem/all.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <optional>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>

namespace {
void check(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

struct Payload {
  std::array<unsigned char, 64> bytes{};
  inline static std::atomic<int> alive{0};
  inline static std::atomic<int> destroyed{0};
  Payload(bool fail = false) {
    if (fail) {
      throw std::runtime_error("shared constructor failure");
    }
    bytes.fill(117);
    alive.fetch_add(1);
  }
  ~Payload() noexcept {
    alive.fetch_sub(1);
    destroyed.fetch_add(1);
  }
};
static_assert(sizeof(Payload) == 64 && alignof(Payload) == 1);

// The explicit context is constructed first and destroyed after all late owners.
unimem::Global late_memory(compiled_test::global_mode);
struct LateOwners {
  std::shared_ptr<Payload> strong;
  std::weak_ptr<Payload> weak;
  std::optional<unimem::OwnedBlock> block;
  ~LateOwners() noexcept {
    auto& memory = late_memory;
    strong.reset();
    if (!weak.expired()) {
      std::_Exit(81);
    }
    weak.reset();
    block.reset();
    if (memory.statistics().live_bytes != 0) {
      std::_Exit(82);
    }
    void* pointer = memory.resource()->allocate(3, 16);
    memory.resource()->deallocate(pointer, 3, 16);
  }
} late_owners;

struct NativeRequest {
  inline static std::size_t bytes = 0, alignment = 0, allocations = 0, frees = 0;
};

void* native_allocate(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CONFIG_BACKEND == 0
  void* pointer = alignment > alignof(void*) ? ::operator new(bytes, std::align_val_t{alignment}, std::nothrow) : ::operator new(bytes, std::nothrow);
#elif UNIMEMORY_CONFIG_BACKEND == 1
  void* pointer = alignment > alignof(void*) ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes);
#elif UNIMEMORY_CONFIG_BACKEND == 2
  void* pointer = je_mallocx(bytes, MALLOCX_ALIGN(alignment));
#else
#error "Global shared regression requires the selected build profile"
#endif
  if (!pointer) {
    throw std::bad_alloc();
  }
  NativeRequest::bytes = bytes;
  NativeRequest::alignment = alignment;
  ++NativeRequest::allocations;
  return pointer;
}

void native_free(void* pointer, std::size_t alignment) noexcept {
  if (!pointer) {
    return;
  }
#if UNIMEMORY_CONFIG_BACKEND == 0
  if (alignment > alignof(void*)) {
    ::operator delete(pointer, std::align_val_t{alignment});
  } else {
    ::operator delete(pointer);
  }
#elif UNIMEMORY_CONFIG_BACKEND == 1
  (void)alignment;
  mi_free(pointer);
#elif UNIMEMORY_CONFIG_BACKEND == 2
  (void)alignment;
  je_dallocx(pointer, 0);
#endif
  ++NativeRequest::frees;
}

template <class T>
class NativeAllocator {
  struct Storage {
    static void* operator new(std::size_t bytes, std::align_val_t alignment) { return native_allocate(bytes, static_cast<std::size_t>(alignment)); }
  };

 public:
  using value_type = T;
  using is_always_equal = std::true_type;
  template <class U>
  struct rebind {
    using other = NativeAllocator<U>;
  };
  NativeAllocator() noexcept = default;
  template <class U>
  NativeAllocator(const NativeAllocator<U>&) noexcept {}
  T* allocate(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      throw std::length_error("native storage overflow");
    }
    if (count == 0) {
      return static_cast<T*>(native_allocate(1, alignof(T)));
    }
    void* storage = Storage::operator new(count * sizeof(T), std::align_val_t{alignof(T)});
    return *static_cast<T(*)[]>(storage);
  }
  void deallocate(T* pointer, std::size_t) noexcept { native_free(pointer, alignof(T)); }
  template <class U>
  bool operator==(const NativeAllocator<U>&) const noexcept {
    return true;
  }
};
static_assert(std::is_empty_v<NativeAllocator<Payload>>);

void snapshot(unimem::Memory& memory, const unimem::Statistics& baseline, std::size_t allocations, std::size_t frees, std::size_t live,
              std::size_t peak) {
  const auto current = memory.statistics();
  check(compiled_test::tracking(), "Basic snapshot missing");
  check(current.allocations == baseline.allocations + allocations, "allocation count mismatch");
  check(current.deallocations == baseline.deallocations + frees, "free count mismatch");
  check(current.reallocations == baseline.reallocations, "shared operation recorded a reallocation");
  check(current.live_bytes == live && current.peak_live_bytes == peak, "requested bytes/peak mismatch");
}

void run(bool require_binding_shape) {
  compiled_test::verify_configuration();
  unimem::Global memory(compiled_test::global_mode);
  const auto initial = memory.statistics();
  check(compiled_test::tracking() == (compiled_test::supports_statistics), "Global statistics mode mismatch");
  check(!compiled_test::tracking() || initial.live_bytes == 0, "fixture requires an isolated Global ledger");
  if (require_binding_shape && !compiled_test::tracking()) {
    throw std::runtime_error("binding_shape requires statistics ON");
  }

  {
    auto reference = std::allocate_shared<Payload>(NativeAllocator<Payload>{}, false);
    check(reference->bytes.front() == 117 && reference.use_count() == 1, "native reference lifetime");
  }
  check(NativeRequest::allocations == 1 && NativeRequest::frees == 1 && NativeRequest::bytes >= sizeof(Payload), "native request shape not observed");
  check(Payload::alive.load() == 0, "native object retained");
  const auto destructor_baseline = Payload::destroyed.load();

  auto strong = memory.make_shared<Payload>(false);
  std::weak_ptr<Payload> weak = strong;
  const auto allocated = memory.statistics();
  check(compiled_test::tracking() || compiled_test::zero(allocated), "Disabled shared statistics changed");
  const auto request = compiled_test::tracking() ? allocated.live_bytes - initial.live_bytes : 0;
  check(!compiled_test::tracking() || request >= sizeof(Payload), "shared allocation did not record its real storage bytes");
  const auto peak = compiled_test::tracking() ? std::max(initial.peak_live_bytes, initial.live_bytes + request) : 0;
  if (compiled_test::tracking()) {
    snapshot(memory, initial, 1, 0, initial.live_bytes + request, peak);
  }
  std::cout << "native_stateless_request_bytes=" << NativeRequest::bytes << " native_request_alignment=" << NativeRequest::alignment
            << " api_request_bytes=";
  if (compiled_test::tracking()) {
    std::cout << request;
  } else {
    std::cout << "unobservable_OFF";
  }
  std::cout << " payload_bytes=" << sizeof(Payload) << '\n';

  std::mutex mutex;
  std::condition_variable changed;
  bool parked = false, release = false;
  std::thread worker([owner = std::move(strong), &mutex, &changed, &parked, &release]() mutable {
    owner.reset();
    std::unique_lock lock(mutex);
    parked = true;
    changed.notify_all();
    changed.wait(lock, [&] { return release; });
  });
  struct JoinOnExit {
    std::thread& worker;
    std::mutex& mutex;
    std::condition_variable& changed;
    bool& release;
    ~JoinOnExit() {
      {
        std::lock_guard lock(mutex);
        release = true;
      }
      changed.notify_all();
      if (worker.joinable()) {
        worker.join();
      }
    }
  } cleanup{worker, mutex, changed, release};
  {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return parked; });
  }
  check(!strong && weak.expired() && !weak.lock(), "cross-thread last strong release failed");
  check(Payload::alive.load() == 0 && Payload::destroyed.load() == destructor_baseline + 1,
        "payload destructor did not complete before parked snapshot");
  if (compiled_test::tracking()) {
    snapshot(memory, initial, 1, 0, initial.live_bytes + request, peak);
  }
  weak.reset();
  if (compiled_test::tracking()) {
    snapshot(memory, initial, 1, 1, initial.live_bytes, peak);
  }
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  changed.notify_all();
  worker.join();

  const auto rollback_before = memory.statistics();
  bool threw = false;
  try {
    (void)memory.make_shared<Payload>(true);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  check(threw && Payload::alive.load() == 0 && Payload::destroyed.load() == destructor_baseline + 1, "shared constructor failure lifetime mismatch");
  if (compiled_test::tracking()) {
    snapshot(memory, rollback_before, 1, 1, rollback_before.live_bytes,
             std::max(rollback_before.peak_live_bytes, rollback_before.live_bytes + request));
  }

  // Raw native storage survives a Global scope; owners/adapters require its lifetime.
  void* surviving = nullptr;
  {
    unimem::Global transient;
    surviving = transient.allocate(32, 16);
    std::fill_n(static_cast<unsigned char*>(surviving), 32, 91);
  }
  check(static_cast<unsigned char*>(surviving)[0] == 91 && static_cast<unsigned char*>(surviving)[31] == 91,
        "Global destruction released raw native storage");
  {
    unimem::Global release_context;
    release_context.deallocate(surviving, 32, 16);
  }

  // Existing full suites cover array/const/overaligned/Heap/Stack shared.
  // Keep the public typed overflow failure observable in this new ledger.
  const auto overflow_before = memory.statistics();
  bool overflow = false;
  try {
    (void)memory.allocate_objects<Payload>(std::numeric_limits<std::size_t>::max() / sizeof(Payload) + 1);
  } catch (const std::length_error&) {
    overflow = true;
  }
  check(overflow, "typed count overflow was accepted");
  if (compiled_test::tracking()) {
    snapshot(memory, overflow_before, 0, 0, overflow_before.live_bytes, overflow_before.peak_live_bytes);
  }
  if (require_binding_shape) {
    unimem::Global repeated(unimem::StatisticsMode::Disabled);
    check(&repeated != &memory && repeated.resource() != memory.resource() && !repeated.resource()->is_equal(*memory.resource()),
          "independent Global resources share a context binding");
    auto ignored = repeated.make_shared<Payload>();
    check(compiled_test::zero(repeated.statistics()), "Disabled Global peer statistics changed");
    unimem::Heap peer(compiled_test::global_mode);
    check(peer.resource() != memory.resource() && !peer.resource()->is_equal(*memory.resource()),
          "Global and Heap PMR resources must bind distinct contexts");
    auto first = memory.make_shared<Payload>();
    auto second = peer.make_shared<Payload>();
    std::weak_ptr<Payload> first_weak = first, second_weak = second;
    const auto first_live = memory.statistics().live_bytes;
    const auto second_live = peer.statistics().live_bytes;
    check(first_live != 0 && second_live != 0, "shared storage must enter each owning context ledger");
    first.reset();
    check(first_weak.expired() && memory.statistics().live_bytes == first_live && peer.statistics().live_bytes == second_live,
          "weak storage binding changed Heap ledger");
    first_weak.reset();
    check(memory.statistics().live_bytes == 0 && peer.statistics().live_bytes == second_live, "first control block freed through wrong context");
    second.reset();
    check(second_weak.expired() && peer.statistics().live_bytes == second_live, "peer weak storage released early");
    second_weak.reset();
    check(peer.statistics().live_bytes == 0, "peer control block retained");
  }
  auto& late = late_owners;
  late.strong = late_memory.make_shared<Payload>();
  late.weak = late.strong;
  late.block.emplace(late_memory.make_block(16));
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const std::string_view mode = argc == 1 ? "lifetime" : argv[1];
    check(argc <= 2 && (mode == "lifetime" || mode == "binding_shape"), "use lifetime or binding_shape");
    run(mode == "binding_shape");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Global shared regression: " << error.what() << '\n';
    return 1;
  }
}
