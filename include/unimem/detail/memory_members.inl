    template<class T>
    void construct_array_with_rollback(T* objects, std::size_t count);

    template<class T>
    class GlobalAllocator;
    static void* global_shared_allocate(std::size_t bytes, std::size_t alignment);
    static void global_shared_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept;

    bool is_global() const noexcept;
    template<bool Zero>
    static void* global_allocate_raw(std::size_t bytes, std::size_t alignment) noexcept(detail::build_backend != Backend::Standard);
    static void global_deallocate_raw(void* pointer, std::size_t alignment) noexcept;
    static void* global_reallocate_raw(void* pointer, std::size_t old_bytes,
                                         std::size_t new_bytes, std::size_t alignment) noexcept(detail::build_backend != Backend::Standard);

    void* cold_allocate(std::size_t bytes, std::size_t alignment) noexcept;
    void* cold_allocate_zeroed(std::size_t bytes, std::size_t alignment) noexcept;
    void* cold_reallocate(void* pointer, std::size_t old_bytes,
                          std::size_t new_bytes, std::size_t alignment) noexcept;
    void cold_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept;

    Memory(StatisticsMode statistics, MemoryKind kind);
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

        explicit Storage(Backend value) noexcept;
        Storage(void* buffer, std::size_t capacity) noexcept;
    };

    static void* stack_allocate(void*, std::size_t, std::size_t) noexcept;
    static void* stack_reallocate(void*, void*, std::size_t, std::size_t,
                                  std::size_t) noexcept;
    static void stack_deallocate(void*, void*, std::size_t, std::size_t) noexcept;
    static void stack_destroy(void*) noexcept;

    // Global PMR callbacks add no per-Memory instance data.
    class GlobalResource final : public std::pmr::memory_resource {
    public:
        GlobalResource() noexcept = default;
    private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override;
        void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;
    };
    static_assert(sizeof(GlobalResource) == sizeof(std::pmr::memory_resource));
    static_assert(alignof(GlobalResource) == alignof(std::pmr::memory_resource));
    struct GlobalResourceStorage {
        alignas(GlobalResource) std::byte bytes[sizeof(GlobalResource)];
    };
    static GlobalResourceStorage global_resource_storage_;

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
    const MemoryKind kind_;
    Storage storage_;
    Resource resource_;
