#pragma once

#include <unimem/common.h>
#include <unimem/owned_block.h>
#include <unimem/allocator.h>
#include <unimem/smart_ptr.h>

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>

namespace unimem {

class Memory final {
public:
    static Memory& global();
    static Memory heap(StatisticsMode statistics = StatisticsMode::Disabled);
    static Memory stack(void* buffer, std::size_t capacity);
    static Memory stack(std::span<std::byte> buffer);

    ~Memory();
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;
    Memory(Memory&&) = delete;
    Memory& operator=(Memory&&) = delete;

    MemoryKind kind() const noexcept;

    std::optional<Backend> backend() const noexcept;

    MemoryCapabilities capabilities() const noexcept;

    void* allocate(std::size_t bytes,
                   std::size_t alignment = alignof(std::max_align_t));
    void* allocate_zeroed(std::size_t bytes,
                         std::size_t alignment = alignof(std::max_align_t));
    void* reallocate(void* pointer, std::size_t old_bytes,
                     std::size_t new_bytes,
                     std::size_t alignment = alignof(std::max_align_t));
    void* reallocate_zeroed(void* pointer, std::size_t old_bytes,
                           std::size_t new_bytes,
                           std::size_t alignment = alignof(std::max_align_t));
    void deallocate(void* pointer, std::size_t bytes,
                    std::size_t alignment = alignof(std::max_align_t)) noexcept;

    OwnedBlock make_block(std::size_t bytes,
                         std::size_t alignment = alignof(std::max_align_t));

    template<class T>
    T* allocate_objects(std::size_t count = 1);

    template<class T>
    void deallocate_objects(T* pointer, std::size_t count = 1) noexcept;

    template<class T, class... Args>
    T* create(Args&&... args);

    template<class T>
    void destroy(T* pointer) noexcept;

    template<class T>
    T* create_array(std::size_t count);

    template<class T>
    void destroy_array(T* pointer, std::size_t count) noexcept;

    template<class T, class... Args>
    Unique<T> make_unique(Args&&... args);

    template<class T>
    UniqueArray<T> make_unique_array(std::size_t count);

    template<class T, class... Args>
    std::shared_ptr<T> make_shared(Args&&... args);

    template<class T>
    Unique<T> adopt_unique(T* pointer) noexcept;

    template<class T>
    UniqueArray<T> adopt_unique_array(T* pointer, std::size_t count) noexcept;

    std::pmr::memory_resource* resource() noexcept;

    template<class T>
    Allocator<T> allocator() noexcept;

    std::optional<MemoryStatistics> statistics() const noexcept;
    std::optional<BackendStatistics> backend_statistics() const;

    void reset();
    void collect();
    bool owns(const void* pointer) const;

    class Mark;

    Mark mark() const;

    void rewind(Mark mark);

    std::size_t used() const;

    std::size_t capacity() const;

private:
#include <unimem/detail/memory_members.inl>
};

}

#include <unimem/detail/memory.inl>
#include <unimem/detail/global_storage.inl>
#include <unimem/detail/memory_core.inl>
