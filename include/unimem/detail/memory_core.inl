#pragma once


#include <unimem/detail/backend.h>

#include <algorithm>
#include <climits>
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

namespace memory_impl {

UNIMEMORY_FORCE_INLINE bool valid_alignment(std::size_t alignment) noexcept {
    return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

struct GlobalSlot {
    std::mutex mutex;
    std::atomic<Memory*> instance{nullptr};
};

inline GlobalSlot& global_slot() {
    // Keep mutex/publication state alive across static destruction, without
    // hidden Heap allocation or an automatically destroyed Memory object.
    alignas(GlobalSlot) static std::byte storage[sizeof(GlobalSlot)];
    static GlobalSlot* initialized = ::new (storage) GlobalSlot;
    (void)initialized;
    return *std::launder(reinterpret_cast<GlobalSlot*>(storage));
}

inline void validate_statistics(StatisticsMode mode) {
    if (mode != StatisticsMode::Disabled && mode != StatisticsMode::Basic) {
        throw std::invalid_argument("UniMemory: invalid statistics mode");
    }
}

UNIMEMORY_FORCE_INLINE void update_peak(detail::TrackingContext* tracked, std::uint64_t live) noexcept {
    auto peak = tracked->peak_live_bytes.load(std::memory_order_relaxed);
    while (live > peak && !tracked->peak_live_bytes.compare_exchange_weak(
               peak, live, std::memory_order_relaxed)) {}
}

UNIMEMORY_FORCE_INLINE void record_allocation(detail::TrackingContext* tracked, std::size_t bytes) noexcept {
    tracked->allocations.fetch_add(1, std::memory_order_relaxed);
    const auto live = tracked->live_bytes.fetch_add(bytes, std::memory_order_relaxed)
        + bytes;
    update_peak(tracked, live);
}

UNIMEMORY_FORCE_INLINE void record_reallocation(detail::TrackingContext* tracked,
    std::size_t old_bytes, std::size_t new_bytes) noexcept {
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

UNIMEMORY_FORCE_INLINE void record_deallocation(detail::TrackingContext* tracked,
    std::size_t bytes) noexcept {
    tracked->deallocations.fetch_add(1, std::memory_order_relaxed);
    tracked->live_bytes.fetch_sub(bytes, std::memory_order_relaxed);
}

UNIMEMORY_FORCE_INLINE void* tracked_allocate(void* context, std::size_t bytes,
                       std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    void* pointer = tracked->base.ops->allocate(tracked->base.context, bytes, alignment);
    if (pointer != nullptr) { record_allocation(tracked, bytes); }
    return pointer;
}

UNIMEMORY_FORCE_INLINE void* tracked_allocate_zeroed(void* context, std::size_t bytes,
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

UNIMEMORY_FORCE_INLINE void* tracked_reallocate(void* context, void* pointer, std::size_t old_bytes,
                         std::size_t new_bytes, std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    void* next = nullptr;
    if (tracked->base.ops->reallocate != nullptr) {
        next = tracked->base.ops->reallocate(tracked->base.context, pointer,
                                             old_bytes, new_bytes, alignment);
    } else {
        next = tracked->base.ops->allocate(tracked->base.context, new_bytes, alignment);
        if (next != nullptr) {
            std::memcpy(next, pointer, (std::min)(old_bytes, new_bytes));
            tracked->base.ops->deallocate(tracked->base.context, pointer,
                                           old_bytes, alignment);
        }
    }
    if (next != nullptr) {
        record_reallocation(tracked, old_bytes, new_bytes);
    }
    return next;
}

UNIMEMORY_FORCE_INLINE void tracked_deallocate(void* context, void* pointer, std::size_t bytes,
                        std::size_t alignment) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    tracked->base.ops->deallocate(tracked->base.context, pointer, bytes, alignment);
    record_deallocation(tracked, bytes);
}

inline void tracked_destroy(void* context) noexcept {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    tracked->base.ops->destroy(tracked->base.context);
    delete tracked;
}

inline bool tracked_statistics(void* context, BackendStatistics& result) {
    auto* tracked = static_cast<detail::TrackingContext*>(context);
    return tracked->base.ops->statistics != nullptr &&
           tracked->base.ops->statistics(tracked->base.context, result);
}

inline const detail::BackendOps tracked_ops{tracked_allocate, tracked_allocate_zeroed,
                                     tracked_reallocate, tracked_deallocate,
                                     tracked_destroy, tracked_statistics};

}

// Constant-initialized bytes have no constructor/destructor/atexit guard.
// The placement-created PMR object itself is never automatically destroyed.
inline constinit Memory::GlobalResourceStorage Memory::global_resource_storage_{};

UNIMEMORY_FORCE_INLINE Memory& Memory::global(Backend backend) {
    if (backend != selected_backend) {
        throw std::invalid_argument("UniMemory: backend differs from this build's selected backend");
    }
    auto& slot = memory_impl::global_slot();
    if (slot.instance.load(std::memory_order_acquire) == nullptr) {
        std::lock_guard lock(slot.mutex);
        if (slot.instance.load(std::memory_order_relaxed) == nullptr) {
            Memory* instance = ::new (detail::global_storage.bytes)
                Memory(selected_backend, selected_statistics, MemoryKind::Global);
            // Memory/SDK/tracking are live; the noexcept stateless PMR ctor
            // only establishes its vptr, and precedes release publication.
            (void)::new (global_resource_storage_.bytes) GlobalResource;
            slot.instance.store(instance, std::memory_order_release);
        }
    }
    // A successful acquire or guarded initialization proves a live object.
    return *std::launder(reinterpret_cast<Memory*>(detail::global_storage.bytes));
}

inline Memory Memory::heap(Backend backend, StatisticsMode statistics) {
    return Memory(backend, statistics, MemoryKind::Heap);
}

inline Memory Memory::stack(void* buffer, std::size_t capacity) {
    return Memory(buffer, capacity);
}

inline Memory::Memory(Backend backend, StatisticsMode statistics, MemoryKind kind)
    : kind_(kind), storage_(backend), resource_(*this) {
    if (backend != selected_backend) {
        throw std::invalid_argument("UniMemory: backend differs from this build's selected backend");
    }
    if constexpr (selected_statistics == StatisticsMode::Disabled) {
        if (statistics == StatisticsMode::Basic) {
            throw std::invalid_argument("UniMemory: Basic statistics are disabled in this build");
        }
    }
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
        ops_ = &memory_impl::tracked_ops;
        context_ = tracking_;
    } else {
        ops_ = handle.ops;
        context_ = handle.context;
    }
}

inline Memory::~Memory() { ops_->destroy(context_); }

inline Memory::Memory(void* buffer, std::size_t capacity)
    : kind_(MemoryKind::Stack), storage_(buffer, capacity), resource_(*this) {
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    if ((buffer == nullptr && capacity != 0) ||
        capacity > (std::numeric_limits<std::uintptr_t>::max)() - address) {
        throw std::invalid_argument("UniMemory: invalid stack buffer");
    }
    static const detail::BackendOps ops{stack_allocate, nullptr, stack_reallocate,
                                      stack_deallocate, stack_destroy, nullptr};
    ops_ = &ops;
    context_ = &storage_.stack;
}

UNIMEMORY_FORCE_INLINE void* Memory::stack_allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept {
    auto& stack = *static_cast<StackStorage*>(context);
    const auto address = reinterpret_cast<std::uintptr_t>(stack.buffer) + stack.used;
    const auto padding = (alignment - (address & (alignment - 1))) & (alignment - 1);
    if (padding > stack.capacity - stack.used || bytes > stack.capacity - stack.used - padding) {
        return nullptr;
    }
    stack.used += padding + bytes;
    return reinterpret_cast<void*>(address + padding);
}

UNIMEMORY_FORCE_INLINE void* Memory::stack_reallocate(void* context, void* pointer, std::size_t old_bytes,
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

UNIMEMORY_FORCE_INLINE void Memory::stack_deallocate(void*, void*, std::size_t, std::size_t) noexcept {}
inline void Memory::stack_destroy(void*) noexcept {}

inline MemoryCapabilities Memory::capabilities() const noexcept {
    MemoryCapabilities result;
    const auto* native_ops = tracking_ == nullptr ? ops_ : tracking_->base.ops;
    result.basic_statistics = selected_statistics == StatisticsMode::Basic && kind_ != MemoryKind::Stack;
    result.detailed_statistics = kind_ != MemoryKind::Stack &&
        native_ops->statistics != nullptr &&
        unimem::capabilities(storage_.backend).detailed_statistics;
    result.reset = kind_ != MemoryKind::Global;
    result.collect = result.owns = kind_ == MemoryKind::Heap;
    result.checkpoints = kind_ == MemoryKind::Stack;
    result.thread_safe = result.individual_reclaim = kind_ != MemoryKind::Stack;
    return result;
}

inline void Memory::reset() {
    if (kind_ == MemoryKind::Stack) {
        if (storage_.stack.generation == (std::numeric_limits<std::uint64_t>::max)()) {
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

inline void Memory::collect() {
    if (kind_ != MemoryKind::Heap) {
        throw std::logic_error("UniMemory: collect requires Heap Memory");
    }
    const auto handle = tracking_ == nullptr ? detail::BackendHandle{ops_, context_}
                                            : tracking_->base;
    handle.ops->collect(handle.context);
}

inline bool Memory::owns(const void* pointer) const {
    if (kind_ != MemoryKind::Heap) {
        throw std::logic_error("UniMemory: owns requires Heap Memory");
    }
    if (pointer == nullptr) { return false; }
    const auto handle = tracking_ == nullptr ? detail::BackendHandle{ops_, context_}
                                            : tracking_->base;
    return handle.ops->owns(handle.context, pointer);
}

inline std::optional<MemoryStatistics> Memory::statistics() const noexcept {
    if (tracking_ == nullptr) { return std::nullopt; }
    return MemoryStatistics{
        tracking_->allocations.load(std::memory_order_relaxed),
        tracking_->deallocations.load(std::memory_order_relaxed),
        tracking_->reallocations.load(std::memory_order_relaxed),
        tracking_->live_bytes.load(std::memory_order_relaxed),
        tracking_->peak_live_bytes.load(std::memory_order_relaxed)};
}

inline std::optional<BackendStatistics> Memory::backend_statistics() const {
    if (ops_->statistics == nullptr) { return std::nullopt; }
    BackendStatistics result;
    if (!ops_->statistics(context_, result)) { return std::nullopt; }
    return result;
}

#include <unimem/detail/allocation.inl>

// Global shared storage follows the same SDK and exact accounting as Global PMR.
UNIMEMORY_FORCE_INLINE void* Memory::global_shared_allocate(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
    if (!memory_impl::valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
#endif
    const auto effective_bytes = bytes == 0 ? 1 : bytes;
    void* pointer = Memory::global_allocate_raw<false>(effective_bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    if constexpr (Memory::selected_statistics == StatisticsMode::Basic) {
        auto* memory = std::launder(reinterpret_cast<Memory*>(detail::global_storage.bytes));
        memory_impl::record_allocation(memory->tracking_, effective_bytes);
    }
    return pointer;
}

UNIMEMORY_FORCE_INLINE void Memory::global_shared_deallocate(
    void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
    if (pointer == nullptr) { return; }
    Memory::global_deallocate_raw(pointer, alignment);
    if constexpr (Memory::selected_statistics == StatisticsMode::Basic) {
        auto* memory = std::launder(reinterpret_cast<Memory*>(detail::global_storage.bytes));
        memory_impl::record_deallocation(memory->tracking_, bytes == 0 ? 1 : bytes);
    }
}

// Callbacks operate on the published Global instance.
UNIMEMORY_FORCE_INLINE void* Memory::GlobalResource::do_allocate(std::size_t bytes, std::size_t alignment) {
#if UNIMEMORY_CHECKS
    if (!memory_impl::valid_alignment(alignment)) {
        throw std::invalid_argument("UniMemory: alignment must be a power of two");
    }
#endif
    const auto effective_bytes = bytes == 0 ? 1 : bytes;
    void* pointer = Memory::global_allocate_raw<false>(effective_bytes, alignment);
    if (pointer == nullptr) { throw std::bad_alloc(); }
    if constexpr (Memory::selected_statistics == StatisticsMode::Basic) {
        auto* memory = std::launder(reinterpret_cast<Memory*>(detail::global_storage.bytes));
        memory_impl::record_allocation(memory->tracking_, effective_bytes);
    }
    return pointer;
}

UNIMEMORY_FORCE_INLINE void Memory::GlobalResource::do_deallocate(
    void* pointer, std::size_t bytes, std::size_t alignment) {
    if (pointer == nullptr) { return; }
    Memory::global_deallocate_raw(pointer, alignment);
    if constexpr (Memory::selected_statistics == StatisticsMode::Basic) {
        auto* memory = std::launder(reinterpret_cast<Memory*>(detail::global_storage.bytes));
        memory_impl::record_deallocation(memory->tracking_, bytes == 0 ? 1 : bytes);
    }
}

inline bool Memory::GlobalResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
    return this == &other;
}


UNIMEMORY_FORCE_INLINE OwnedBlock Memory::make_block(std::size_t bytes, std::size_t alignment) {
    return OwnedBlock(*this, allocate(bytes, alignment), bytes, alignment);
}

UNIMEMORY_FORCE_INLINE OwnedBlock::OwnedBlock(Memory& memory, void* pointer, std::size_t bytes,
                       std::size_t alignment) noexcept
    : memory_(&memory), pointer_(pointer), bytes_(bytes), alignment_(alignment) {}

UNIMEMORY_FORCE_INLINE OwnedBlock::~OwnedBlock() { clear(); }

UNIMEMORY_FORCE_INLINE OwnedBlock::OwnedBlock(OwnedBlock&& other) noexcept
    : memory_(std::exchange(other.memory_, nullptr)),
      pointer_(std::exchange(other.pointer_, nullptr)),
      bytes_(std::exchange(other.bytes_, 0)),
      alignment_(std::exchange(other.alignment_, alignof(std::max_align_t))) {}

UNIMEMORY_FORCE_INLINE OwnedBlock& OwnedBlock::operator=(OwnedBlock&& other) noexcept {
    if (this != &other) {
        clear();
        memory_ = std::exchange(other.memory_, nullptr);
        pointer_ = std::exchange(other.pointer_, nullptr);
        bytes_ = std::exchange(other.bytes_, 0);
        alignment_ = std::exchange(other.alignment_, alignof(std::max_align_t));
    }
    return *this;
}

UNIMEMORY_FORCE_INLINE void OwnedBlock::resize(std::size_t new_bytes) {
    if (memory_ == nullptr) { throw std::logic_error("UniMemory: moved-from block"); }
    void* next = memory_->reallocate(pointer_, bytes_, new_bytes, alignment_);
    pointer_ = next;
    bytes_ = new_bytes;
}

UNIMEMORY_FORCE_INLINE void OwnedBlock::clear() noexcept {
    // Private legal states bind every nonempty block to its Memory.
    if (pointer_ != nullptr) {
#if UNIMEMORY_CHECKS
        if (memory_ == nullptr) {
            assert(memory_ != nullptr && "UniMemory: nonempty OwnedBlock must have a bound Memory");
            std::terminate();
        }
#endif
        memory_->deallocate(pointer_, bytes_, alignment_);
    }
    memory_ = nullptr;
    pointer_ = nullptr;
    bytes_ = 0;
    alignment_ = alignof(std::max_align_t);
}

}
