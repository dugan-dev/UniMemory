#include <unimem/memory.h>

#include "backend.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>

namespace unimem {

namespace detail {

struct TrackingContext {
    BackendHandle base;
    std::atomic<std::uint64_t> allocations{0};
    std::atomic<std::uint64_t> deallocations{0};
    std::atomic<std::uint64_t> reallocations{0};
    std::atomic<std::uint64_t> live_bytes{0};
    std::atomic<std::uint64_t> peak_live_bytes{0};
};

}

namespace {

bool valid_alignment(std::size_t alignment) noexcept {
    return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

union GlobalStorage {
    std::byte bytes[sizeof(Memory)];
    std::max_align_t alignment;
};
static_assert(alignof(GlobalStorage) >= alignof(Memory));

struct GlobalSlot {
    std::mutex mutex;
    std::atomic<Memory*> instance{nullptr};
    StatisticsMode statistics = StatisticsMode::Disabled;
    GlobalStorage storage{};
};

GlobalSlot& global_slot(Backend backend) {
    switch (backend) {
    case Backend::Standard:
    case Backend::Mimalloc:
    case Backend::Jemalloc: break;
    default: throw std::invalid_argument("UniMemory: invalid backend");
    }
    if (!available(backend)) { throw std::runtime_error("UniMemory: backend is not enabled"); }
    // The registry intentionally survives static destruction, without a hidden
    // startup Heap allocation. It also retains optional Global counter state.
    struct Registry { GlobalSlot slots[3]; };
    alignas(Registry) static std::byte storage[sizeof(Registry)];
    static Registry* registry = ::new (storage) Registry;
    return registry->slots[static_cast<unsigned>(backend)];
}

void validate_statistics(StatisticsMode mode) {
    if (mode != StatisticsMode::Disabled && mode != StatisticsMode::Basic) {
        throw std::invalid_argument("UniMemory: invalid statistics mode");
    }
}

void update_peak(detail::TrackingContext* tracked, std::uint64_t live) noexcept {
    auto peak = tracked->peak_live_bytes.load(std::memory_order_relaxed);
    while (live > peak && !tracked->peak_live_bytes.compare_exchange_weak(
               peak, live, std::memory_order_relaxed)) {}
}

void record_allocation(detail::TrackingContext* tracked, std::size_t bytes) noexcept {
    tracked->allocations.fetch_add(1, std::memory_order_relaxed);
    const auto live = tracked->live_bytes.fetch_add(bytes, std::memory_order_relaxed)
        + bytes;
    update_peak(tracked, live);
}

void* tracked_allocate(void* context, std::size_t bytes,
                       std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    void* pointer = tracked->base.ops->allocate(tracked->base.context, bytes, alignment);
    if (pointer != nullptr) { record_allocation(tracked, bytes); }
    return pointer;
}

void* tracked_allocate_zeroed(void* context, std::size_t bytes,
                              std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    void* pointer = tracked->base.ops->allocate_zeroed != nullptr
        ? tracked->base.ops->allocate_zeroed(tracked->base.context, bytes, alignment)
        : tracked->base.ops->allocate(tracked->base.context, bytes, alignment);
    if (pointer != nullptr) {
        if (tracked->base.ops->allocate_zeroed == nullptr) {
            std::memset(pointer, 0, bytes);
        }
        record_allocation(tracked, bytes);
    }
    return pointer;
}

void* tracked_reallocate(void* context, void* pointer, std::size_t old_bytes,
                         std::size_t new_bytes, std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    void* next = nullptr;
    if (tracked->base.ops->reallocate != nullptr) {
        next = tracked->base.ops->reallocate(tracked->base.context, pointer,
                                             old_bytes, new_bytes, alignment);
    } else {
        next = tracked->base.ops->allocate(tracked->base.context, new_bytes, alignment);
        if (next != nullptr) {
            std::memcpy(next, pointer, std::min(old_bytes, new_bytes));
            tracked->base.ops->deallocate(tracked->base.context, pointer,
                                           old_bytes, alignment);
        }
    }
    if (next != nullptr) {
        tracked->reallocations.fetch_add(1, std::memory_order_relaxed);
        if (new_bytes > old_bytes) {
            const auto live = tracked->live_bytes.fetch_add(new_bytes - old_bytes,
                                                             std::memory_order_relaxed)
                + (new_bytes - old_bytes);
            update_peak(tracked, live);
        } else {
            tracked->live_bytes.fetch_sub(old_bytes - new_bytes,
                                           std::memory_order_relaxed);
        }
    }
    return next;
}

void tracked_deallocate(void* context, void* pointer, std::size_t bytes,
                        std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    tracked->base.ops->deallocate(tracked->base.context, pointer, bytes, alignment);
    tracked->deallocations.fetch_add(1, std::memory_order_relaxed);
    tracked->live_bytes.fetch_sub(bytes, std::memory_order_relaxed);
}

void tracked_destroy(void* context) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    tracked->base.ops->destroy(tracked->base.context);
    delete tracked;
}

bool tracked_statistics(void* context, BackendStatistics& result) {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    return tracked->base.ops->statistics != nullptr &&
           tracked->base.ops->statistics(tracked->base.context, result);
}

const detail::BackendOps tracked_ops{tracked_allocate, tracked_allocate_zeroed,
                                     tracked_reallocate, tracked_deallocate,
                                     tracked_destroy, tracked_statistics};

}

bool available(Backend backend) noexcept {
    switch (backend) {
    case Backend::Standard: return true;
    case Backend::Mimalloc:
#ifdef UNIMEMORY_WITH_MIMALLOC
        return true;
#else
        return false;
#endif
    case Backend::Jemalloc:
#ifdef UNIMEMORY_WITH_JEMALLOC
        return true;
#else
        return false;
#endif
    default: return false;
    }
}

BackendCapabilities capabilities(Backend backend) noexcept {
    if (!available(backend)) { return {}; }
    switch (backend) {
    case Backend::Standard: return {true, false, false, false};
    case Backend::Mimalloc: return {true, true, true, true};
    case Backend::Jemalloc:
#ifdef UNIMEMORY_WITH_JEMALLOC
        return {true, detail::jemalloc_statistics_available(), true, true};
#else
        return {};
#endif
    default: return {};
    }
}

bool supports(Backend backend, RuntimeOption option) noexcept {
    return option == RuntimeOption::UnusedPageReleaseDelayMs &&
           capabilities(backend).release_delay;
}

bool set_runtime_option(Backend backend, RuntimeOption option, std::int64_t value) {
    if (option == RuntimeOption::UnusedPageReleaseDelayMs &&
        (value < -1 ||
         value > static_cast<std::int64_t>(std::numeric_limits<long>::max()))) {
        throw std::invalid_argument("UniMemory: invalid release delay");
    }
    if (!supports(backend, option)) { return false; }
    bool result = false;
    switch (backend) {
    case Backend::Mimalloc:
#ifdef UNIMEMORY_WITH_MIMALLOC
        result = detail::set_mimalloc_release_delay(value);
#endif
        break;
    case Backend::Jemalloc:
#ifdef UNIMEMORY_WITH_JEMALLOC
        result = detail::set_jemalloc_release_delay(value);
#endif
        break;
    default: break;
    }
    if (!result) { throw std::runtime_error("UniMemory: backend rejected release delay"); }
    return true;
}

Memory& Memory::global(Backend backend) {
    auto& slot = global_slot(backend);
    if (Memory* instance = slot.instance.load(std::memory_order_acquire)) { return *instance; }
    std::lock_guard lock(slot.mutex);
    Memory* instance = slot.instance.load(std::memory_order_relaxed);
    if (instance == nullptr) {
        instance = ::new (slot.storage.bytes) Memory(backend, slot.statistics, MemoryKind::Global);
        slot.instance.store(instance, std::memory_order_release);
    }
    return *instance;
}

void Memory::configure_global(Backend backend, StatisticsMode statistics) {
    validate_statistics(statistics);
    auto& slot = global_slot(backend);
    std::lock_guard lock(slot.mutex);
    if (slot.instance.load(std::memory_order_relaxed) != nullptr && slot.statistics != statistics) {
        throw std::logic_error("UniMemory: global Memory is already initialized");
    }
    slot.statistics = statistics;
}

Memory Memory::heap(Backend backend, StatisticsMode statistics) {
    return Memory(backend, statistics, MemoryKind::Heap);
}

Memory Memory::stack(void* buffer, std::size_t capacity) {
    return Memory(buffer, capacity);
}

Memory::Memory(Backend backend, StatisticsMode statistics, MemoryKind kind)
    : kind_(kind), storage_(backend), resource_(*this) {
    const bool dedicated_arena = kind == MemoryKind::Heap;
    if (statistics != StatisticsMode::Disabled &&
        statistics != StatisticsMode::Basic) {
        throw std::invalid_argument("UniMemory: invalid statistics mode");
    }
    detail::BackendHandle handle{};
    switch (backend) {
    case Backend::Standard: handle = detail::system_backend(dedicated_arena); break;
    case Backend::Mimalloc:
#ifdef UNIMEMORY_WITH_MIMALLOC
        handle = detail::mimalloc_backend(dedicated_arena); break;
#else
        throw std::runtime_error("UniMemory: mimalloc is not enabled");
#endif
    case Backend::Jemalloc:
#ifdef UNIMEMORY_WITH_JEMALLOC
        handle = detail::jemalloc_backend(dedicated_arena); break;
#else
        throw std::runtime_error("UniMemory: jemalloc is not enabled");
#endif
    default: throw std::invalid_argument("UniMemory: invalid backend");
    }
    if (statistics == StatisticsMode::Basic) {
        try {
            tracking_ = new detail::TrackingContext{handle};
        } catch (...) {
            handle.ops->destroy(handle.context);
            throw;
        }
        ops_ = &tracked_ops;
        context_ = tracking_;
    } else {
        ops_ = handle.ops;
        context_ = handle.context;
    }
}

Memory::~Memory() { ops_->destroy(context_); }

Memory::Memory(void* buffer, std::size_t capacity)
    : kind_(MemoryKind::Stack), storage_(buffer, capacity), resource_(*this) {
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    if ((buffer == nullptr && capacity != 0) ||
        capacity > std::numeric_limits<std::uintptr_t>::max() - address) {
        throw std::invalid_argument("UniMemory: invalid stack buffer");
    }
    static const detail::BackendOps ops{stack_allocate, nullptr, stack_reallocate,
                                      stack_deallocate, stack_destroy, nullptr};
    ops_ = &ops;
    context_ = &storage_.stack;
}

void* Memory::stack_allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept {
    auto& stack = *static_cast<StackStorage*>(context);
    const auto address = reinterpret_cast<std::uintptr_t>(stack.buffer) + stack.used;
    const auto padding = (alignment - (address & (alignment - 1))) & (alignment - 1);
    if (padding > stack.capacity - stack.used || bytes > stack.capacity - stack.used - padding) {
        return nullptr;
    }
    stack.used += padding + bytes;
    return reinterpret_cast<void*>(address + padding);
}

void* Memory::stack_reallocate(void* context, void* pointer, std::size_t old_bytes,
                                std::size_t new_bytes, std::size_t alignment) noexcept {
    if (new_bytes <= old_bytes) { return pointer; }
    auto& stack = *static_cast<StackStorage*>(context);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto end = reinterpret_cast<std::uintptr_t>(stack.buffer) + stack.used;
    if (address <= end && old_bytes == end - address &&
        new_bytes - old_bytes <= stack.capacity - stack.used) {
        stack.used += new_bytes - old_bytes;
        return pointer;
    }
    void* next = stack_allocate(context, new_bytes, alignment);
    if (next != nullptr) { std::memcpy(next, pointer, old_bytes); }
    return next;
}

void Memory::stack_deallocate(void*, void*, std::size_t, std::size_t) noexcept {}
void Memory::stack_destroy(void*) noexcept {}

MemoryCapabilities Memory::capabilities() const noexcept {
    MemoryCapabilities result;
    const auto* native_ops = tracking_ == nullptr ? ops_ : tracking_->base.ops;
    result.basic_statistics = kind_ != MemoryKind::Stack;
    result.detailed_statistics = kind_ != MemoryKind::Stack &&
        native_ops->statistics != nullptr &&
        unimem::capabilities(storage_.backend).detailed_statistics;
    result.reset = kind_ != MemoryKind::Global;
    result.collect = result.owns = kind_ == MemoryKind::Heap;
    result.checkpoints = kind_ == MemoryKind::Stack;
    result.thread_safe = result.individual_reclaim = kind_ != MemoryKind::Stack;
    return result;
}

void Memory::reset() {
    if (kind_ == MemoryKind::Stack) {
        if (storage_.stack.generation == std::numeric_limits<std::uint64_t>::max()) {
            throw std::length_error("UniMemory: stack mark generation exhausted");
        }
        storage_.stack.used = 0;
        ++storage_.stack.generation;
        return;
    }
    if (kind_ != MemoryKind::Heap) {
        throw std::logic_error("UniMemory: reset requires Heap or Stack Memory");
    }
    auto handle = tracking_ == nullptr ? detail::BackendHandle{ops_, context_}
                                      : tracking_->base;
    void* next = handle.ops->reset(handle.context);
    if (tracking_ == nullptr) {
        context_ = next;
    } else {
        tracking_->base.context = next;
        tracking_->allocations.store(0, std::memory_order_relaxed);
        tracking_->deallocations.store(0, std::memory_order_relaxed);
        tracking_->reallocations.store(0, std::memory_order_relaxed);
        tracking_->live_bytes.store(0, std::memory_order_relaxed);
        tracking_->peak_live_bytes.store(0, std::memory_order_relaxed);
    }
}

void Memory::collect() {
    if (kind_ != MemoryKind::Heap) {
        throw std::logic_error("UniMemory: collect requires Heap Memory");
    }
    const auto handle = tracking_ == nullptr ? detail::BackendHandle{ops_, context_}
                                            : tracking_->base;
    handle.ops->collect(handle.context);
}

bool Memory::owns(const void* pointer) const {
    if (kind_ != MemoryKind::Heap) {
        throw std::logic_error("UniMemory: owns requires Heap Memory");
    }
    if (pointer == nullptr) { return false; }
    const auto handle = tracking_ == nullptr ? detail::BackendHandle{ops_, context_}
                                            : tracking_->base;
    return handle.ops->owns(handle.context, pointer);
}

std::optional<MemoryStatistics> Memory::statistics() const noexcept {
    if (tracking_ == nullptr) { return std::nullopt; }
    return MemoryStatistics{
        tracking_->allocations.load(std::memory_order_relaxed),
        tracking_->deallocations.load(std::memory_order_relaxed),
        tracking_->reallocations.load(std::memory_order_relaxed),
        tracking_->live_bytes.load(std::memory_order_relaxed),
        tracking_->peak_live_bytes.load(std::memory_order_relaxed)};
}

std::optional<BackendStatistics> Memory::backend_statistics() const {
    if (ops_->statistics == nullptr) { return std::nullopt; }
    BackendStatistics result;
    if (!ops_->statistics(context_, result)) { return std::nullopt; }
    return result;
}

void* Memory::allocate(std::size_t bytes, std::size_t alignment) {
    if (!valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
    if (bytes == 0) { return nullptr; }
    void* pointer = ops_->allocate(context_, bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    return pointer;
}

void* Memory::allocate_zeroed(std::size_t bytes, std::size_t alignment) {
    if (!valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
    if (bytes == 0) { return nullptr; }
    void* pointer = ops_->allocate_zeroed != nullptr
        ? ops_->allocate_zeroed(context_, bytes, alignment)
        : ops_->allocate(context_, bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    if (ops_->allocate_zeroed == nullptr) { std::memset(pointer, 0, bytes); }
    return pointer;
}

void* Memory::reallocate(void* pointer, std::size_t old_bytes,
                         std::size_t new_bytes, std::size_t alignment) {
    if (!valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
    if (pointer == nullptr) { return allocate(new_bytes, alignment); }
    if (new_bytes == 0) {
        deallocate(pointer, old_bytes, alignment);
        return nullptr;
    }
    if (ops_->reallocate != nullptr) {
        void* next = ops_->reallocate(context_, pointer, old_bytes,
                                      new_bytes, alignment);
        if (next == nullptr) { throw std::bad_alloc(); }
        return next;
    }
    void* next = allocate(new_bytes, alignment);
    std::memcpy(next, pointer, std::min(old_bytes, new_bytes));
    deallocate(pointer, old_bytes, alignment);
    return next;
}

void* Memory::reallocate_zeroed(void* pointer, std::size_t old_bytes,
                                std::size_t new_bytes, std::size_t alignment) {
    const auto preserved = pointer == nullptr ? 0 : old_bytes;
    void* next = reallocate(pointer, old_bytes, new_bytes, alignment);
    if (next != nullptr && new_bytes > preserved) {
        std::memset(static_cast<std::byte*>(next) + preserved, 0,
                    new_bytes - preserved);
    }
    return next;
}

void Memory::deallocate(void* pointer, std::size_t bytes,
                        std::size_t alignment) noexcept {
    if (pointer != nullptr) {
        ops_->deallocate(context_, pointer, bytes, alignment);
    }
}

OwnedBlock Memory::make_block(std::size_t bytes, std::size_t alignment) {
    return OwnedBlock(*this, allocate(bytes, alignment), bytes, alignment);
}

OwnedBlock::OwnedBlock(Memory& memory, void* pointer, std::size_t bytes,
                       std::size_t alignment) noexcept
    : memory_(&memory), pointer_(pointer), bytes_(bytes), alignment_(alignment) {}

OwnedBlock::~OwnedBlock() { clear(); }

OwnedBlock::OwnedBlock(OwnedBlock&& other) noexcept
    : memory_(std::exchange(other.memory_, nullptr)),
      pointer_(std::exchange(other.pointer_, nullptr)),
      bytes_(std::exchange(other.bytes_, 0)),
      alignment_(std::exchange(other.alignment_, alignof(std::max_align_t))) {}

OwnedBlock& OwnedBlock::operator=(OwnedBlock&& other) noexcept {
    if (this != &other) {
        clear();
        memory_ = std::exchange(other.memory_, nullptr);
        pointer_ = std::exchange(other.pointer_, nullptr);
        bytes_ = std::exchange(other.bytes_, 0);
        alignment_ = std::exchange(other.alignment_, alignof(std::max_align_t));
    }
    return *this;
}

void OwnedBlock::resize(std::size_t new_bytes) {
    if (memory_ == nullptr) { throw std::logic_error("UniMemory: moved-from block"); }
    void* next = memory_->reallocate(pointer_, bytes_, new_bytes, alignment_);
    pointer_ = next;
    bytes_ = new_bytes;
}

void OwnedBlock::clear() noexcept {
    if (memory_ != nullptr && pointer_ != nullptr) {
        memory_->deallocate(pointer_, bytes_, alignment_);
    }
    memory_ = nullptr;
    pointer_ = nullptr;
    bytes_ = 0;
    alignment_ = alignof(std::max_align_t);
}

}
