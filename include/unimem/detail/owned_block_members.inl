    friend class Memory;
    OwnedBlock(Memory& memory, void* pointer, std::size_t bytes,
               std::size_t alignment) noexcept;
    void clear() noexcept;

    Memory* memory_ = nullptr;
    void* pointer_ = nullptr;
    std::size_t bytes_ = 0;
    std::size_t alignment_ = alignof(std::max_align_t);
