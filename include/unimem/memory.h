#pragma once

#include <unimem/common.h>
#include <unimem/owned_block.h>
#include <unimem/allocator.h>
#include <unimem/smart_ptr.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <utility>

namespace unimem {

class Memory final {
public:
    static Memory& global(Backend backend = Backend::Standard);
    static Memory heap(Backend backend,
                       StatisticsMode statistics = StatisticsMode::Disabled);
    static Memory stack(void* buffer, std::size_t capacity);
    static Memory stack(std::span<std::byte> buffer);

    static void configure_global(Backend backend, StatisticsMode statistics);

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

    class Mark final {
    public:
        Mark(const Mark&) = default;
        Mark& operator=(const Mark&) = default;

    private:
        friend class Memory;
        Mark(const Memory* owner, std::size_t used, std::uint64_t generation) noexcept;

        const Memory* owner_;
        std::size_t used_;
        std::uint64_t generation_;
    };

    Mark mark() const;

    void rewind(Mark mark);

    std::size_t used() const;

    std::size_t capacity() const;

private:
    Memory(Backend backend, StatisticsMode statistics, MemoryKind kind);
    Memory(void* buffer, std::size_t capacity);

    void require_stack() const;

    struct StackStorage {
        std::byte* buffer;
        std::size_t capacity;
        std::size_t used = 0;
        std::uint64_t generation = 0;
    };

    union Storage {
        Backend backend;
        StackStorage stack;

        explicit Storage(Backend value) noexcept : backend(value) {}
        Storage(void* buffer, std::size_t capacity) noexcept
            : stack{static_cast<std::byte*>(buffer), capacity} {}
    };

    static void* stack_allocate(void*, std::size_t, std::size_t) noexcept;
    static void* stack_reallocate(void*, void*, std::size_t, std::size_t,
                                  std::size_t) noexcept;
    static void stack_deallocate(void*, void*, std::size_t, std::size_t) noexcept;
    static void stack_destroy(void*) noexcept;

    class Resource final : public std::pmr::memory_resource {
    public:
        explicit Resource(Memory& memory) noexcept;

    private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override;

        void do_deallocate(void* pointer, std::size_t bytes,
                           std::size_t alignment) override;

        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

        Memory& memory_;
    };

    const detail::BackendOps* ops_ = nullptr;
    void* context_ = nullptr;
    detail::TrackingContext* tracking_ = nullptr;
    MemoryKind kind_;
    Storage storage_;
    Resource resource_;
};

}

#include <unimem/detail/memory.inl>
