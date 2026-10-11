#pragma once

#include <cassert>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace unimem {

namespace detail {

struct TypedStorage {
    static UNIMEMORY_FORCE_INLINE void* operator new(std::size_t bytes, std::align_val_t alignment, Memory& memory) {
        return memory.allocate(bytes, static_cast<std::size_t>(alignment));
    }
};

template<class T>
UNIMEMORY_FORCE_INLINE T* allocate_array_storage(Memory& memory, std::size_t count) {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(T)) {
        throw std::length_error("UniMemory: allocation size overflow");
    }
    if (count == 0) { return nullptr; }
    // C++20 [intro.object]/13 applies to explicit operator-new calls, including
    // on reused Stack storage. The implicit array exists before its elements
    // are constructed; array-to-pointer conversion supplies their storage.
    void* storage = TypedStorage::operator new(count * sizeof(T),
                                               std::align_val_t{alignof(T)}, memory);
    return *static_cast<T(*)[]>(storage);
}

}

inline Memory Memory::stack(std::span<std::byte> buffer) {
    return stack(buffer.data(), buffer.size());
}

inline MemoryKind Memory::kind() const noexcept { return kind_; }

inline std::optional<Backend> Memory::backend() const noexcept {
    return kind_ == MemoryKind::Stack
        ? std::nullopt
        : std::optional{storage_.backend};
}

template<class T>
UNIMEMORY_FORCE_INLINE T* Memory::allocate_objects(std::size_t count) {
    return detail::allocate_array_storage<T>(*this, count);
}

template<class T>
UNIMEMORY_FORCE_INLINE void Memory::deallocate_objects(T* pointer, std::size_t count) noexcept {
    deallocate(const_cast<std::remove_cv_t<T>*>(pointer),
               count * sizeof(T), alignof(T));
}

template<class T, class... Args>
UNIMEMORY_FORCE_INLINE T* Memory::create(Args&&... args) {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
    void* storage = allocate(sizeof(T), alignof(T));
    if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
        return ::new (storage) T(std::forward<Args>(args)...);
    } else {
    try {
        return ::new (storage) T(std::forward<Args>(args)...);
    } catch (...) {
        deallocate(storage, sizeof(T), alignof(T));
        throw;
    }
    }
}

template<class T>
UNIMEMORY_FORCE_INLINE void Memory::destroy(T* pointer) noexcept {
    static_assert(std::is_nothrow_destructible_v<T>);
    if (pointer != nullptr) {
        pointer->~T();
        deallocate(const_cast<std::remove_cv_t<T>*>(pointer), sizeof(T), alignof(T));
    }
}

template<class T>
inline void Memory::construct_array_with_rollback(T* objects, std::size_t count) {
    std::size_t built = 0;
    try {
        for (; built < count; ++built) {
            ::new (const_cast<std::remove_cv_t<T>*>(objects + built)) T();
        }
    } catch (...) {
        while (built != 0) { objects[--built].~T(); }
        deallocate_objects(objects, count);
        throw;
    }
}

template<class T>
UNIMEMORY_FORCE_INLINE T* Memory::create_array(std::size_t count) {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
    T* objects = allocate_objects<T>(count);
    if constexpr (std::is_nothrow_default_constructible_v<T>) {
        for (std::size_t built = 0; built < count; ++built) {
            ::new (const_cast<std::remove_cv_t<T>*>(objects + built)) T();
        }
    } else {
        construct_array_with_rollback<T>(objects, count);
    }
    return objects;
}

template<class T>
UNIMEMORY_FORCE_INLINE void Memory::destroy_array(T* pointer, std::size_t count) noexcept {
    static_assert(std::is_nothrow_destructible_v<T>);
    if (pointer != nullptr) {
        const auto original_count = count;
        while (count != 0) { pointer[--count].~T(); }
        deallocate_objects(pointer, original_count);
    }
}

template<class T, class... Args>
UNIMEMORY_FORCE_INLINE Unique<T> Memory::make_unique(Args&&... args) {
    return adopt_unique(create<T>(std::forward<Args>(args)...));
}

template<class T>
UNIMEMORY_FORCE_INLINE UniqueArray<T> Memory::make_unique_array(std::size_t count) {
    return adopt_unique_array(create_array<T>(count), count);
}

// Private Global binding is implicit and immortal; standard control blocks
// can compress this empty allocator. Heap/Stack keep their explicit binding.
template<class T>
class Memory::GlobalAllocator {
    struct TypedStorage {
        static UNIMEMORY_FORCE_INLINE void* operator new(std::size_t bytes, std::align_val_t alignment) {
            return Memory::global_shared_allocate(bytes, static_cast<std::size_t>(alignment));
        }
    };
public:
    using value_type = T;
    using is_always_equal = std::true_type;
    template<class U> struct rebind { using other = GlobalAllocator<U>; };
    GlobalAllocator() noexcept = default;
    template<class U> GlobalAllocator(const GlobalAllocator<U>&) noexcept {}

    UNIMEMORY_FORCE_INLINE T* allocate(std::size_t count) {
        static_assert(std::is_object_v<T> && !std::is_array_v<T>);
        if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(T)) {
            throw std::length_error("UniMemory: allocation size overflow");
        }
        // Zero count is pairable as one byte, without an implicit T array.
        if (count == 0) {
            return static_cast<T*>(Memory::global_shared_allocate(1, alignof(T)));
        }
        // The explicit allocation-function call establishes C++20 array
        // lifetime before allocator_traits constructs the rebound objects.
        void* storage = TypedStorage::operator new(count * sizeof(T), std::align_val_t{alignof(T)});
        return *static_cast<T(*)[]>(storage);
    }
    UNIMEMORY_FORCE_INLINE void deallocate(T* pointer, std::size_t count) noexcept {
        Memory::global_shared_deallocate(const_cast<std::remove_cv_t<T>*>(pointer),
                                        count == 0 ? 1 : count * sizeof(T), alignof(T));
    }
    template<class U> bool operator==(const GlobalAllocator<U>&) const noexcept { return true; }
};

template<class T, class... Args>
UNIMEMORY_FORCE_INLINE std::shared_ptr<T> Memory::make_shared(Args&&... args) {
    static_assert(std::is_nothrow_destructible_v<std::remove_all_extents_t<T>>);
    if (is_global()) {
        static_assert(std::is_empty_v<GlobalAllocator<T>>);
        static_assert(std::allocator_traits<GlobalAllocator<T>>::is_always_equal::value);
        return std::allocate_shared<T>(GlobalAllocator<T>{}, std::forward<Args>(args)...);
    }
    return std::allocate_shared<T>(allocator<T>(), std::forward<Args>(args)...);
}

template<class T>
UNIMEMORY_FORCE_INLINE Unique<T> Memory::adopt_unique(T* pointer) noexcept {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
    return Unique<T>(pointer, Deleter<T>{this});
}

template<class T>
UNIMEMORY_FORCE_INLINE UniqueArray<T> Memory::adopt_unique_array(T* pointer, std::size_t count) noexcept {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
    return UniqueArray<T>(pointer, ArrayDeleter<T>{this, count});
}

UNIMEMORY_FORCE_INLINE std::pmr::memory_resource* Memory::resource() noexcept {
    if (is_global()) {
        // Global acquisition proves both objects live; no local static guard.
        return std::launder(reinterpret_cast<GlobalResource*>(global_resource_storage_.bytes));
    }
    return &resource_;
}

template<class T>
UNIMEMORY_FORCE_INLINE Allocator<T> Memory::allocator() noexcept { return Allocator<T>(*this); }

inline Memory::Mark::Mark(const Memory* owner, std::size_t used,
                         std::uint64_t generation) noexcept
    : owner_(owner), used_(used), generation_(generation) {}

inline Memory::Mark Memory::mark() const {
    require_stack();
    return Mark(this, storage_.stack.used, storage_.stack.generation);
}

inline void Memory::rewind(Mark mark) {
    require_stack();
    StackStorage& stack = storage_.stack;
    if (mark.owner_ != this ||
        mark.generation_ != stack.generation ||
        mark.used_ > stack.used) {
        throw std::invalid_argument("UniMemory: invalid stack mark");
    }
    if (stack.generation == (std::numeric_limits<std::uint64_t>::max)()) {
        throw std::length_error("UniMemory: stack mark generation exhausted");
    }
    stack.used = mark.used_;
    ++stack.generation;
}

inline std::size_t Memory::used() const {
    require_stack();
    return storage_.stack.used;
}

inline std::size_t Memory::capacity() const {
    require_stack();
    return storage_.stack.capacity;
}

inline void Memory::require_stack() const {
    if (kind_ != MemoryKind::Stack) {
        throw std::logic_error("UniMemory: operation requires Stack Memory");
    }
}

inline Memory::Resource::Resource(Memory& memory) noexcept : memory_(memory) {}

UNIMEMORY_FORCE_INLINE void* Memory::Resource::do_allocate(std::size_t bytes, std::size_t alignment) {
    return memory_.allocate(bytes == 0 ? 1 : bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void Memory::Resource::do_deallocate(
    void* pointer, std::size_t bytes, std::size_t alignment) {
    memory_.deallocate(pointer, bytes == 0 ? 1 : bytes, alignment);
}

inline bool Memory::Resource::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
    return this == &other;
}

template<class T>
UNIMEMORY_FORCE_INLINE T* Allocator<T>::allocate(std::size_t count) {
    // A zero-count adapter allocation remains pairable without creating an array.
    if (count == 0) { return static_cast<T*>(memory_->allocate(1, alignof(T))); }
    return detail::allocate_array_storage<T>(*memory_, count);
}

template<class T>
UNIMEMORY_FORCE_INLINE void Allocator<T>::deallocate(T* pointer, std::size_t count) noexcept {
    memory_->deallocate(const_cast<std::remove_cv_t<T>*>(pointer),
                        count == 0 ? 1 : count * sizeof(T), alignof(T));
}

template<class T>
UNIMEMORY_FORCE_INLINE void Deleter<T>::operator()(T* pointer) const noexcept {
    if (pointer == nullptr) { return; }
#if UNIMEMORY_CHECKS
    if (memory == nullptr) {
        assert(memory != nullptr && "UniMemory: nonempty Unique requires a bound Memory");
        std::terminate();
    }
#endif
    memory->destroy(pointer);
}

template<class T>
UNIMEMORY_FORCE_INLINE void ArrayDeleter<T>::operator()(T* pointer) const noexcept {
    if (pointer == nullptr) { return; }
#if UNIMEMORY_CHECKS
    if (memory == nullptr || count == 0) {
        assert(memory != nullptr && count != 0 &&
               "UniMemory: nonempty UniqueArray requires its Memory and count");
        std::terminate();
    }
#endif
    memory->destroy_array(pointer, count);
}

}
