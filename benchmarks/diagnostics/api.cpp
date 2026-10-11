#include "compiled-bench-config.h"
#include "api.h"

#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif
#ifdef UNIMEMORY_BENCH_MIMALLOC
#include <mimalloc.h>
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4068)
#endif
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif

namespace unimem_diagnostics {
namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t raw_bytes = 64;
constexpr std::size_t grown_bytes = 128;
constexpr std::size_t raw_alignment = alignof(std::max_align_t);
constexpr std::size_t array_count = 8;
constexpr std::size_t vector_count = 256;
constexpr std::size_t warm_iterations = 256;

// Alignment 1 deliberately exercises the production unaligned allocation
// branch. All typed allocations use alignof(T), including rebound shared_ptr
// control blocks; raw byte APIs use their normal max_align_t default.
struct Payload64 {
    std::array<unsigned char, raw_bytes> bytes{};
};
static_assert(sizeof(Payload64) == raw_bytes && alignof(Payload64) == 1);
static_assert(std::is_nothrow_destructible_v<Payload64>);

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void escape_pointer(const void* pointer) noexcept {
#if defined(_MSC_VER)
    // A per-thread volatile sink keeps the pointer observable without a shared
    // write or a timing-loop atomic. The compiler barrier also orders contents.
    static thread_local const void* volatile observed = nullptr;
    observed = pointer;
    _ReadWriteBarrier();
#elif defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(pointer) : "memory");
#else
    static thread_local const void* volatile observed = nullptr;
    observed = pointer;
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

void* checked(void* pointer) {
    if (pointer == nullptr) { throw std::bad_alloc(); }
    return pointer;
}

// These are direct equivalents of the Global, statistics-disabled production
// backend entry points. No malloc substitute or backend-specific zero-growth
// shortcut is used in the Standard or reallocate_zeroed controls.
struct NativeStandard {
    void* allocate(std::size_t bytes, std::size_t alignment) const {
        return checked(alignment > alignof(void*)
            ? ::operator new(bytes, std::align_val_t{alignment}, std::nothrow)
            : ::operator new(bytes, std::nothrow));
    }
    void* allocate_zeroed(std::size_t bytes, std::size_t alignment) const {
        void* pointer = allocate(bytes, alignment);
        std::memset(pointer, 0, bytes);
        return pointer;
    }
    void* reallocate(void* pointer, std::size_t old_bytes,
                     std::size_t new_bytes, std::size_t alignment) const {
        void* next = allocate(new_bytes, alignment);
        std::memcpy(next, pointer, std::min(old_bytes, new_bytes));
        deallocate(pointer, old_bytes, alignment);
        return next;
    }
    void deallocate(void* pointer, std::size_t, std::size_t alignment) const noexcept {
        if (alignment > alignof(void*)) {
            ::operator delete(pointer, std::align_val_t{alignment});
        } else {
            ::operator delete(pointer);
        }
    }
};

#ifdef UNIMEMORY_BENCH_MIMALLOC
struct NativeMimalloc {
    void* allocate(std::size_t bytes, std::size_t alignment) const {
        return checked(alignment > alignof(void*)
            ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes));
    }
    void* allocate_zeroed(std::size_t bytes, std::size_t alignment) const {
        return checked(alignment > alignof(void*)
            ? mi_zalloc_aligned(bytes, alignment) : mi_zalloc(bytes));
    }
    void* reallocate(void* pointer, std::size_t, std::size_t new_bytes,
                     std::size_t alignment) const {
        return checked(mi_realloc_aligned(pointer, new_bytes, alignment));
    }
    void deallocate(void* pointer, std::size_t, std::size_t) const noexcept {
        mi_free(pointer);
    }
};
#endif

#ifdef UNIMEMORY_BENCH_JEMALLOC
struct NativeJemalloc {
    NativeJemalloc() {
        // Match the explicit production bootstrap before any timed work.
        const char* version = nullptr;
        std::size_t length = sizeof(version);
        require(je_mallctl("version", &version, &length, nullptr, 0) == 0 &&
                    length == sizeof(version) && version != nullptr,
                "api diagnostic jemalloc initialization failed");
    }
    void* allocate(std::size_t bytes, std::size_t alignment) const {
        return checked(alignment > static_cast<std::size_t>(INT_MAX)
            ? nullptr : je_mallocx(bytes, MALLOCX_ALIGN(alignment)));
    }
    void* allocate_zeroed(std::size_t bytes, std::size_t alignment) const {
        return checked(alignment > static_cast<std::size_t>(INT_MAX)
            ? nullptr : je_mallocx(bytes, MALLOCX_ALIGN(alignment) | MALLOCX_ZERO));
    }
    void* reallocate(void* pointer, std::size_t, std::size_t new_bytes,
                     std::size_t alignment) const {
        return checked(alignment > static_cast<std::size_t>(INT_MAX)
            ? nullptr : je_rallocx(pointer, new_bytes, MALLOCX_ALIGN(alignment)));
    }
    void deallocate(void* pointer, std::size_t, std::size_t) const noexcept {
        je_dallocx(pointer, 0);
    }
};
#endif

template<class Native>
struct NativeTypedStorage {
    static void* operator new(std::size_t bytes, std::align_val_t alignment,
                              Native& native) {
        return native.allocate(bytes, static_cast<std::size_t>(alignment));
    }
};

template<class T, class Native>
T* native_array_storage(Native& native, std::size_t count) {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
        throw std::length_error("api diagnostic allocation size overflow");
    }
    if (count == 0) { return nullptr; }
    // The explicit operator-new call starts the implicit array lifetime, just
    // as unimem::detail::TypedStorage does for Allocator and create_array.
    void* storage = NativeTypedStorage<Native>::operator new(
        count * sizeof(T), std::align_val_t{alignof(T)}, native);
    return *static_cast<T(*)[]>(storage);
}

template<class T, class Native>
class NativeAllocator {
public:
    using value_type = T;
    explicit NativeAllocator(Native& native) noexcept : native_(&native) {}
    template<class U>
    NativeAllocator(const NativeAllocator<U, Native>& other) noexcept
        : native_(&other.native()) {}
    T* allocate(std::size_t count) {
        if (count == 0) { return static_cast<T*>(native_->allocate(1, alignof(T))); }
        return native_array_storage<T>(*native_, count);
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        native_->deallocate(pointer, count == 0 ? 1 : count * sizeof(T), alignof(T));
    }
    Native& native() const noexcept { return *native_; }
    template<class U>
    bool operator==(const NativeAllocator<U, Native>& other) const noexcept {
        return native_ == &other.native();
    }
    template<class U>
    bool operator!=(const NativeAllocator<U, Native>& other) const noexcept {
        return !(*this == other);
    }
private:
    Native* native_;
};

template<class Native>
class NativeResource final : public std::pmr::memory_resource {
public:
    explicit NativeResource(Native& native) noexcept : native_(&native) {}
private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        return native_->allocate(bytes == 0 ? 1 : bytes, alignment);
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        native_->deallocate(pointer, bytes == 0 ? 1 : bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
    Native* native_;
};

template<class Native>
struct NativeDeleter {
    Native* native;
    void operator()(Payload64* pointer) const noexcept {
        if (pointer != nullptr) {
            std::destroy_at(pointer);
            native->deallocate(pointer, sizeof(Payload64), alignof(Payload64));
        }
    }
};

template<class Native>
struct NativeArrayDeleter {
    Native* native;
    std::size_t count;
    void operator()(Payload64* pointer) const noexcept {
        if (pointer != nullptr) {
            auto remaining = count;
            while (remaining != 0) { std::destroy_at(pointer + --remaining); }
            native->deallocate(pointer, count * sizeof(Payload64), alignof(Payload64));
        }
    }
};

template<class Native>
class NativeOwnedBlock final {
public:
    NativeOwnedBlock(Native& native, std::size_t bytes, std::size_t alignment)
        : native_(&native), pointer_(native.allocate(bytes, alignment)),
          bytes_(bytes), alignment_(alignment) {}
    ~NativeOwnedBlock() { native_->deallocate(pointer_, bytes_, alignment_); }
    NativeOwnedBlock(const NativeOwnedBlock&) = delete;
    NativeOwnedBlock& operator=(const NativeOwnedBlock&) = delete;
    void* data() const noexcept { return pointer_; }
    std::size_t size() const noexcept { return bytes_; }
    std::size_t alignment() const noexcept { return alignment_; }
private:
    Native* native_;
    void* pointer_;
    std::size_t bytes_;
    std::size_t alignment_;
};

template<class Native>
struct NativeAccess {
    Native* native;
    void* allocate(std::size_t bytes, std::size_t alignment) {
        return native->allocate(bytes, alignment);
    }
    void* allocate_zeroed(std::size_t bytes, std::size_t alignment) {
        return native->allocate_zeroed(bytes, alignment);
    }
    void* reallocate(void* pointer, std::size_t old_bytes,
                     std::size_t new_bytes, std::size_t alignment) {
        return native->reallocate(pointer, old_bytes, new_bytes, alignment);
    }
    void* reallocate_zeroed(void* pointer, std::size_t old_bytes,
                            std::size_t new_bytes, std::size_t alignment) {
        void* next = reallocate(pointer, old_bytes, new_bytes, alignment);
        std::memset(static_cast<unsigned char*>(next) + old_bytes, 0,
                    new_bytes - old_bytes);
        return next;
    }
    void deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
        native->deallocate(pointer, bytes, alignment);
    }
    auto make_unique() {
        using Owner = std::unique_ptr<Payload64, NativeDeleter<Native>>;
        static_assert(sizeof(Owner) == sizeof(unimem::Unique<Payload64>));
        void* storage = native->allocate(sizeof(Payload64), alignof(Payload64));
        try {
            return Owner(::new (storage) Payload64(), NativeDeleter<Native>{native});
        } catch (...) {
            native->deallocate(storage, sizeof(Payload64), alignof(Payload64));
            throw;
        }
    }
    auto make_array() {
        using Owner = std::unique_ptr<Payload64[], NativeArrayDeleter<Native>>;
        static_assert(sizeof(Owner) == sizeof(unimem::UniqueArray<Payload64>));
        auto* objects = native_array_storage<Payload64>(*native, array_count);
        std::size_t built = 0;
        try {
            for (; built < array_count; ++built) { ::new (objects + built) Payload64(); }
        } catch (...) {
            while (built != 0) { std::destroy_at(objects + --built); }
            native->deallocate(objects, sizeof(Payload64) * array_count, alignof(Payload64));
            throw;
        }
        return Owner(objects, NativeArrayDeleter<Native>{native, array_count});
    }
    auto make_shared() {
        // Same allocation shape and std::shared_ptr implementation as the API.
        // Its reference-count atomics are part of both controls, not API cost.
        static_assert(sizeof(NativeAllocator<Payload64, Native>) ==
                      sizeof(unimem::Allocator<Payload64>));
        return std::allocate_shared<Payload64>(NativeAllocator<Payload64, Native>(*native));
    }
    auto make_owned() {
        static_assert(sizeof(NativeOwnedBlock<Native>) == sizeof(unimem::OwnedBlock));
        return NativeOwnedBlock<Native>(*native, raw_bytes, raw_alignment);
    }
};

struct ActualAccess {
    unimem::Memory* memory;
    void* allocate(std::size_t bytes, std::size_t alignment) {
        return memory->allocate(bytes, alignment);
    }
    void* allocate_zeroed(std::size_t bytes, std::size_t alignment) {
        return memory->allocate_zeroed(bytes, alignment);
    }
    void* reallocate(void* pointer, std::size_t old_bytes,
                     std::size_t new_bytes, std::size_t alignment) {
        return memory->reallocate(pointer, old_bytes, new_bytes, alignment);
    }
    void* reallocate_zeroed(void* pointer, std::size_t old_bytes,
                            std::size_t new_bytes, std::size_t alignment) {
        return memory->reallocate_zeroed(pointer, old_bytes, new_bytes, alignment);
    }
    void deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
        memory->deallocate(pointer, bytes, alignment);
    }
    auto make_unique() { return memory->make_unique<Payload64>(); }
    auto make_array() { return memory->make_unique_array<Payload64>(array_count); }
    auto make_shared() { return memory->make_shared<Payload64>(); }
    auto make_owned() { return memory->make_block(raw_bytes, raw_alignment); }
};

template<class Access>
struct RawOwner {
    Access* access;
    unsigned char* pointer;
    std::size_t bytes;
    ~RawOwner() { access->deallocate(pointer, bytes, raw_alignment); }
};

unsigned char pattern(std::size_t iteration) noexcept {
    return static_cast<unsigned char>(iteration % 251 + 1);
}

void verify_alignment(const void* pointer, std::size_t alignment) {
    require(pointer != nullptr &&
                reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0,
            "api diagnostic allocation/alignment mismatch");
}

void verify_bytes(const unsigned char* pointer, std::size_t bytes,
                  unsigned char value) {
    for (std::size_t index = 0; index < bytes; ++index) {
        require(pointer[index] == value, "api diagnostic content mismatch");
    }
}

template<bool Verify, class Access>
std::uint64_t zeroed_step(Access& access, std::size_t iteration) {
    RawOwner<Access> owner{&access, static_cast<unsigned char*>(
        access.allocate_zeroed(raw_bytes, raw_alignment)), raw_bytes};
    escape_pointer(owner.pointer);
    if constexpr (Verify) {
        verify_alignment(owner.pointer, raw_alignment);
        verify_bytes(owner.pointer, raw_bytes, 0);
    }
    const auto initial = static_cast<std::uint64_t>(owner.pointer[0]) +
                         owner.pointer[raw_bytes - 1];
    std::memset(owner.pointer, pattern(iteration), raw_bytes);
    escape_pointer(owner.pointer);
    if constexpr (Verify) { verify_bytes(owner.pointer, raw_bytes, pattern(iteration)); }
    return initial + owner.pointer[0] + owner.pointer[raw_bytes - 1];
}

template<bool Verify, bool ZeroGrowth, class Access>
std::uint64_t reallocate_step(Access& access, std::size_t iteration) {
    RawOwner<Access> owner{&access, static_cast<unsigned char*>(
        access.allocate(raw_bytes, raw_alignment)), raw_bytes};
    std::memset(owner.pointer, pattern(iteration), raw_bytes);
    escape_pointer(owner.pointer);
    if constexpr (Verify) { verify_alignment(owner.pointer, raw_alignment); }
    // Preserve owner.pointer and size if reallocation throws.
    void* next;
    if constexpr (ZeroGrowth) {
        next = access.reallocate_zeroed(owner.pointer, raw_bytes, grown_bytes, raw_alignment);
    } else {
        next = access.reallocate(owner.pointer, raw_bytes, grown_bytes, raw_alignment);
    }
    owner.pointer = static_cast<unsigned char*>(next);
    owner.bytes = grown_bytes;
    escape_pointer(owner.pointer);
    if constexpr (Verify) {
        verify_alignment(owner.pointer, raw_alignment);
        verify_bytes(owner.pointer, raw_bytes, pattern(iteration));
        if constexpr (ZeroGrowth) {
            verify_bytes(owner.pointer + raw_bytes, grown_bytes - raw_bytes, 0);
        }
    }
    std::uint64_t zero_sample = 0;
    if constexpr (ZeroGrowth) {
        zero_sample = static_cast<std::uint64_t>(owner.pointer[raw_bytes]) +
                      owner.pointer[grown_bytes - 1];
    }
    std::memset(owner.pointer + raw_bytes, pattern(iteration) + 1, grown_bytes - raw_bytes);
    escape_pointer(owner.pointer);
    if constexpr (Verify) {
        verify_bytes(owner.pointer + raw_bytes, grown_bytes - raw_bytes,
                     static_cast<unsigned char>(pattern(iteration) + 1));
    }
    return zero_sample + owner.pointer[0] + owner.pointer[raw_bytes - 1] +
           owner.pointer[raw_bytes] + owner.pointer[grown_bytes - 1];
}

template<bool Verify>
std::uint64_t use_payload(Payload64& object, std::size_t iteration) {
    escape_pointer(&object);
    if constexpr (Verify) {
        verify_alignment(&object, alignof(Payload64));
        verify_bytes(object.bytes.data(), object.bytes.size(), 0);
    }
    const auto initial = static_cast<std::uint64_t>(object.bytes.front()) +
                         object.bytes.back();
    std::memset(object.bytes.data(), pattern(iteration), object.bytes.size());
    escape_pointer(&object);
    if constexpr (Verify) { verify_bytes(object.bytes.data(), object.bytes.size(), pattern(iteration)); }
    return initial + object.bytes.front() + object.bytes.back();
}

template<bool Verify, class Access>
std::uint64_t unique_step(Access& access, std::size_t iteration) {
    auto owner = access.make_unique();
    return use_payload<Verify>(*owner, iteration);
}

template<bool Verify, class Access>
std::uint64_t array_step(Access& access, std::size_t iteration) {
    auto owner = access.make_array();
    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < array_count; ++index) {
        checksum += use_payload<Verify>(owner[index], iteration);
    }
    return checksum;
}

template<bool Verify, class Access>
std::uint64_t shared_step(Access& access, std::size_t iteration) {
    auto owner = access.make_shared();
    auto second_owner = owner;
    if constexpr (Verify) {
        require(second_owner.get() == owner.get() && owner.use_count() == 2,
                "api diagnostic shared ownership mismatch");
    }
    return use_payload<Verify>(*second_owner, iteration) +
           static_cast<std::uint64_t>(owner.use_count());
}

template<bool Verify, class Allocator>
std::uint64_t vector_step(Allocator& allocator, std::size_t iteration) {
    using Value = std::uint64_t;
    std::vector<Value, Allocator> values(allocator);
    // Fixed reserve/growth and 256 identical push_back operations in std and
    // pmr controls. Only the allocation adapter changes within each pair.
    values.reserve(vector_count);
    for (std::size_t index = 0; index < vector_count; ++index) {
        values.push_back(static_cast<Value>(pattern(iteration)) + index);
    }
    escape_pointer(values.data());
    if constexpr (Verify) {
        verify_alignment(values.data(), alignof(Value));
        require(values.size() == vector_count && values.capacity() >= vector_count,
                "api diagnostic vector shape mismatch");
        for (std::size_t index = 0; index < vector_count; ++index) {
            require(values[index] == static_cast<Value>(pattern(iteration)) + index,
                    "api diagnostic vector content mismatch");
        }
    }
    return values.front() + values.back() + values.size();
}

template<bool Verify, class Access>
std::uint64_t owned_step(Access& access, std::size_t iteration) {
    auto owner = access.make_owned();
    auto* pointer = static_cast<unsigned char*>(owner.data());
    if constexpr (Verify) {
        verify_alignment(pointer, raw_alignment);
        require(owner.size() == raw_bytes && owner.alignment() == raw_alignment,
                "api diagnostic owned block metadata mismatch");
    }
    std::memset(pointer, pattern(iteration), raw_bytes);
    escape_pointer(pointer);
    if constexpr (Verify) { verify_bytes(pointer, raw_bytes, pattern(iteration)); }
    return static_cast<std::uint64_t>(pointer[0]) + pointer[raw_bytes - 1];
}

enum class Workload { Zeroed, Reallocate, ReallocateZeroed, Unique, Array,
                      Shared, Vector, PmrVector, Owned };

struct Selection {
    Workload workload;
    bool native;
    std::size_t bytes;
};

Selection select(std::string_view variant) {
    const bool native = variant.starts_with("native_");
    if (!native && !variant.starts_with("api_")) {
        throw std::invalid_argument("unknown api diagnostic variant: " + std::string(variant));
    }
    const auto name = variant.substr(native ? 7 : 4);
    if (name == "zeroed") { return {Workload::Zeroed, native, raw_bytes}; }
    if (name == "reallocate") { return {Workload::Reallocate, native, grown_bytes}; }
    if (name == "reallocate_zeroed") { return {Workload::ReallocateZeroed, native, grown_bytes}; }
    if (name == "unique") { return {Workload::Unique, native, sizeof(Payload64)}; }
    if (name == "array") { return {Workload::Array, native, sizeof(Payload64) * array_count}; }
    if (name == "shared") { return {Workload::Shared, native, sizeof(Payload64)}; }
    if (name == "vector") { return {Workload::Vector, native, sizeof(std::uint64_t) * vector_count}; }
    if (name == "pmr_vector") { return {Workload::PmrVector, native, sizeof(std::uint64_t) * vector_count}; }
    if (name == "owned") { return {Workload::Owned, native, raw_bytes}; }
    throw std::invalid_argument("unknown api diagnostic variant: " + std::string(variant));
}

std::uint64_t expected_checksum(Workload workload, std::size_t iterations) noexcept {
    // A closed form keeps post-timing validation independent of measured loop
    // length. Unsigned wrap is intentional and identical to loop accumulation.
    const auto cycles = static_cast<std::uint64_t>(iterations / 251);
    const auto remainder = static_cast<std::uint64_t>(iterations % 251);
    const auto sum = cycles * (251u * 252u / 2u) + remainder * (remainder + 1) / 2;
    const auto count = static_cast<std::uint64_t>(iterations);
    switch (workload) {
    case Workload::Reallocate:
    case Workload::ReallocateZeroed: return 4 * sum + 2 * count;
    case Workload::Array: return 2 * array_count * sum;
    case Workload::Shared: return 2 * sum + 2 * count;
    case Workload::Vector:
    case Workload::PmrVector: return 2 * sum + (2 * vector_count - 1) * count;
    default: return 2 * sum;
    }
}

struct Measurement {
    double seconds;
    std::uint64_t checksum;
};

template<class Step, class Verify>
Measurement measure(Step&& step, Verify&& verify, Workload workload,
                    std::size_t iterations) {
    // Includes every byte, initial value initialization, zeroed extension and
    // container/refcount state, at several seeds. --verify iterations=1 runs
    // the same checks; expensive full scans never enter the timed loop.
    constexpr std::array<std::size_t, 8> samples{0, 1, 2, 63, 249, 250, 251, 502};
    for (const auto sample : samples) { (void)verify(sample); }
    for (std::size_t index = 0; index < warm_iterations; ++index) { (void)step(index); }
    std::uint64_t checksum = 0;
    const auto begin = Clock::now();
    for (std::size_t index = 0; index < iterations; ++index) { checksum += step(index); }
    const auto end = Clock::now();
    for (const auto sample : samples) { (void)verify(sample); }
    const double seconds = std::chrono::duration<double>(end - begin).count();
    require(seconds > 0, "api diagnostic timer resolution too coarse");
    require(checksum == expected_checksum(workload, iterations),
            "api diagnostic content checksum mismatch");
    return {seconds, checksum};
}

template<class Access, class Allocator>
Measurement run_workload(Access& access, Allocator& allocator,
                         std::pmr::memory_resource* resource,
                         Workload workload, std::size_t iterations) {
    switch (workload) {
    case Workload::Zeroed:
        return measure([&](auto i) { return zeroed_step<false>(access, i); },
                       [&](auto i) { return zeroed_step<true>(access, i); }, workload, iterations);
    case Workload::Reallocate:
        return measure([&](auto i) { return reallocate_step<false, false>(access, i); },
                       [&](auto i) { return reallocate_step<true, false>(access, i); }, workload, iterations);
    case Workload::ReallocateZeroed:
        return measure([&](auto i) { return reallocate_step<false, true>(access, i); },
                       [&](auto i) { return reallocate_step<true, true>(access, i); }, workload, iterations);
    case Workload::Unique:
        return measure([&](auto i) { return unique_step<false>(access, i); },
                       [&](auto i) { return unique_step<true>(access, i); }, workload, iterations);
    case Workload::Array:
        return measure([&](auto i) { return array_step<false>(access, i); },
                       [&](auto i) { return array_step<true>(access, i); }, workload, iterations);
    case Workload::Shared:
        return measure([&](auto i) { return shared_step<false>(access, i); },
                       [&](auto i) { return shared_step<true>(access, i); }, workload, iterations);
    case Workload::Vector:
        return measure([&](auto i) { return vector_step<false>(allocator, i); },
                       [&](auto i) { return vector_step<true>(allocator, i); }, workload, iterations);
    case Workload::PmrVector: {
        std::pmr::polymorphic_allocator<std::uint64_t> pmr_allocator(resource);
        return measure([&](auto i) { return vector_step<false>(pmr_allocator, i); },
                       [&](auto i) { return vector_step<true>(pmr_allocator, i); }, workload, iterations);
    }
    case Workload::Owned:
        return measure([&](auto i) { return owned_step<false>(access, i); },
                       [&](auto i) { return owned_step<true>(access, i); }, workload, iterations);
    }
    throw std::logic_error("api diagnostic invalid workload");
}

template<class Native>
void run_backend(std::ostream& output, const std::string& backend_name,
                 unimem::Backend backend, const std::string& variant,
                 const Selection& selection, std::size_t iterations) {
    Measurement result;
    if (selection.native) {
        Native native;
        NativeAccess<Native> access{&native};
        NativeAllocator<std::uint64_t, Native> allocator(native);
        NativeResource<Native> resource(native);
        result = run_workload(access, allocator, &resource, selection.workload, iterations);
    } else {
        compiled_benchmark_configuration(backend, unimem::StatisticsMode::Disabled);
        auto& memory = unimem::Memory::global(backend);
        require(memory.kind() == unimem::MemoryKind::Global &&
                    memory.backend() == backend && !memory.statistics().has_value(),
                "api diagnostic requires cached statistics-disabled Global Memory");
        ActualAccess access{&memory};
        auto allocator = memory.allocator<std::uint64_t>();
        auto* resource = memory.resource();
        result = run_workload(access, allocator, resource, selection.workload, iterations);
    }
    // bytes is logical payload size (grown size for reallocation). Shared_ptr
    // implementation control-block overhead is the same in both variants.
    output << "api," << backend_name << ',' << variant << ',' << selection.bytes
           << ",1," << iterations << ',' << std::setprecision(17) << result.seconds
           << ',' << result.seconds * 1e9 / static_cast<double>(iterations)
           << ",ns/iteration," << result.checksum << '\n';
}

}

void run_api(std::ostream& output, const std::string& backend,
             const std::string& variant, std::size_t iterations) {
    if (iterations == 0) { throw std::invalid_argument("api diagnostic requires positive iterations"); }
    const auto selection = select(variant);
    if (backend == "standard") {
        run_backend<NativeStandard>(output, backend, unimem::Backend::Standard,
                                    variant, selection, iterations);
        return;
    }
#ifdef UNIMEMORY_BENCH_MIMALLOC
    if (backend == "mimalloc") {
        run_backend<NativeMimalloc>(output, backend, unimem::Backend::Mimalloc,
                                   variant, selection, iterations);
        return;
    }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    if (backend == "jemalloc") {
        run_backend<NativeJemalloc>(output, backend, unimem::Backend::Jemalloc,
                                   variant, selection, iterations);
        return;
    }
#endif
    throw std::invalid_argument("api diagnostic backend is unavailable: " + backend);
}

}
