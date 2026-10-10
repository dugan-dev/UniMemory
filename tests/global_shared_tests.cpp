#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
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
    if (!value) { throw std::runtime_error(message); }
}

struct Payload {
    std::array<unsigned char, 64> bytes{};
    inline static std::atomic<int> alive{0};
    inline static std::atomic<int> destroyed{0};
    explicit Payload(bool fail = false) {
        if (fail) { throw std::runtime_error("shared constructor failure"); }
        bytes.fill(117);
        alive.fetch_add(1);
    }
    ~Payload() noexcept { alive.fetch_sub(1); destroyed.fetch_add(1); }
};
static_assert(sizeof(Payload) == 64 && alignof(Payload) == 1);

struct NativeRequest {
    inline static std::size_t bytes = 0, alignment = 0, allocations = 0, frees = 0;
};

void* native_allocate(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CONFIG_BACKEND == 0
    void* pointer = alignment > alignof(void*)
        ? ::operator new(bytes, std::align_val_t{alignment}, std::nothrow)
        : ::operator new(bytes, std::nothrow);
#elif UNIMEMORY_CONFIG_BACKEND == 1
    void* pointer = alignment > alignof(void*) ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes);
#elif UNIMEMORY_CONFIG_BACKEND == 2
    void* pointer = je_mallocx(bytes, MALLOCX_ALIGN(alignment));
#else
#error "Global shared regression requires the selected build profile"
#endif
    if (!pointer) { throw std::bad_alloc(); }
    NativeRequest::bytes = bytes;
    NativeRequest::alignment = alignment;
    ++NativeRequest::allocations;
    return pointer;
}

void native_free(void* pointer, std::size_t alignment) noexcept {
    if (!pointer) { return; }
#if UNIMEMORY_CONFIG_BACKEND == 0
    if (alignment > alignof(void*)) { ::operator delete(pointer, std::align_val_t{alignment}); }
    else { ::operator delete(pointer); }
#elif UNIMEMORY_CONFIG_BACKEND == 1
    (void)alignment;
    mi_free(pointer);
#elif UNIMEMORY_CONFIG_BACKEND == 2
    (void)alignment;
    je_dallocx(pointer, 0);
#endif
    ++NativeRequest::frees;
}

template<class T>
class NativeAllocator {
    struct Storage {
        static void* operator new(std::size_t bytes, std::align_val_t alignment) {
            return native_allocate(bytes, static_cast<std::size_t>(alignment));
        }
    };
public:
    using value_type = T;
    using is_always_equal = std::true_type;
    template<class U> struct rebind { using other = NativeAllocator<U>; };
    NativeAllocator() noexcept = default;
    template<class U> NativeAllocator(const NativeAllocator<U>&) noexcept {}
    T* allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::length_error("native storage overflow");
        }
        if (count == 0) { return static_cast<T*>(native_allocate(1, alignof(T))); }
        void* storage = Storage::operator new(count * sizeof(T), std::align_val_t{alignof(T)});
        return *static_cast<T(*)[]>(storage);
    }
    void deallocate(T* pointer, std::size_t) noexcept { native_free(pointer, alignof(T)); }
    template<class U> bool operator==(const NativeAllocator<U>&) const noexcept { return true; }
};
static_assert(std::is_empty_v<NativeAllocator<Payload>>);

void snapshot(unimem::Memory& memory, const unimem::MemoryStatistics& baseline,
              std::size_t allocations, std::size_t frees, std::size_t live, std::size_t peak) {
    const auto current = memory.statistics();
    check(current.has_value(), "Basic snapshot missing");
    check(current->allocations == baseline.allocations + allocations, "allocation count mismatch");
    check(current->deallocations == baseline.deallocations + frees, "free count mismatch");
    check(current->reallocations == baseline.reallocations, "shared operation recorded a reallocation");
    check(current->live_bytes == live && current->peak_live_bytes == peak, "requested bytes/peak mismatch");
}

void run(bool require_stateless_shape) {
    auto& memory = unimem::Memory::global();
    const auto initial = memory.statistics();
    check(initial.has_value() == (unimem::Memory::selected_statistics == unimem::StatisticsMode::Basic),
          "Global statistics mode mismatch");
    check(!initial || initial->live_bytes == 0, "fixture requires an isolated Global ledger");
    if (require_stateless_shape && !initial) {
        throw std::runtime_error("stateless_shape requires statistics ON; OFF request bytes need actual SDK/ASM evidence");
    }

    {
        auto reference = std::allocate_shared<Payload>(NativeAllocator<Payload>{}, false);
        check(reference->bytes.front() == 117 && reference.use_count() == 1, "native reference lifetime");
    }
    check(NativeRequest::allocations == 1 && NativeRequest::frees == 1 && NativeRequest::bytes >= sizeof(Payload),
          "native request shape not observed");
    check(Payload::alive.load() == 0, "native object retained");
    const auto destructor_baseline = Payload::destroyed.load();

    auto strong = memory.make_shared<Payload>(false);
    std::weak_ptr<Payload> weak = strong;
    const auto allocated = memory.statistics();
    check(allocated.has_value() == initial.has_value(), "shared allocation changed statistics availability");
    const auto request = initial ? allocated->live_bytes - initial->live_bytes : 0;
    check(!initial || request >= sizeof(Payload), "shared allocation did not record its real storage bytes");
    const auto peak = initial ? std::max(initial->peak_live_bytes, initial->live_bytes + request) : 0;
    if (initial) { snapshot(memory, *initial, 1, 0, initial->live_bytes + request, peak); }
    std::cout << "native_stateless_request_bytes=" << NativeRequest::bytes
              << " native_request_alignment=" << NativeRequest::alignment << " api_request_bytes=";
    if (initial) { std::cout << request; }
    else { std::cout << "unobservable_OFF"; }
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
            { std::lock_guard lock(mutex); release = true; }
            changed.notify_all();
            if (worker.joinable()) { worker.join(); }
        }
    } cleanup{worker, mutex, changed, release};
    {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return parked; });
    }
    check(!strong && weak.expired() && !weak.lock(), "cross-thread last strong release failed");
    check(Payload::alive.load() == 0 && Payload::destroyed.load() == destructor_baseline + 1,
          "payload destructor did not complete before parked snapshot");
    if (initial) { snapshot(memory, *initial, 1, 0, initial->live_bytes + request, peak); }
    weak.reset();
    if (initial) { snapshot(memory, *initial, 1, 1, initial->live_bytes, peak); }
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    changed.notify_all();
    worker.join();

    const auto rollback_before = memory.statistics();
    bool threw = false;
    try { (void)memory.make_shared<Payload>(true); }
    catch (const std::runtime_error&) { threw = true; }
    check(threw && Payload::alive.load() == 0 && Payload::destroyed.load() == destructor_baseline + 1,
          "shared constructor failure lifetime mismatch");
    if (rollback_before) {
        snapshot(memory, *rollback_before, 1, 1, rollback_before->live_bytes,
                 std::max(rollback_before->peak_live_bytes, rollback_before->live_bytes + request));
    }

    // Existing full suites cover array/const/overaligned/Heap/Stack shared.
    // Keep the public typed overflow failure observable in this new ledger.
    const auto overflow_before = memory.statistics();
    bool overflow = false;
    try { (void)memory.allocate_objects<Payload>(std::numeric_limits<std::size_t>::max() / sizeof(Payload) + 1); }
    catch (const std::length_error&) { overflow = true; }
    check(overflow, "typed count overflow was accepted");
    if (overflow_before) {
        snapshot(memory, *overflow_before, 0, 0, overflow_before->live_bytes, overflow_before->peak_live_bytes);
    }
    if (require_stateless_shape) {
        check(request == NativeRequest::bytes, "Global shared still requests a larger control block than the native empty allocator");
    }
}
}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc == 1 ? "lifetime" : argv[1];
        check(argc <= 2 && (mode == "lifetime" || mode == "stateless_shape"), "use lifetime or stateless_shape");
        run(mode == "stateless_shape");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Global shared regression: " << error.what() << '\n';
        return 1;
    }
}
