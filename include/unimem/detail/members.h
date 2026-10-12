#pragma once

#include <unimem/detail/backend.h>

#define UNIM_IMPL_MEMORY_MEMBERS                                                                                                           \
  template <class T>                                                                                                                       \
  void construct_array_with_rollback(T* objects, std::size_t count);                                                                       \
                                                                                                                                           \
  template <bool Zero>                                                                                                                     \
  static void* global_allocate_raw(std::size_t bytes, std::size_t alignment) noexcept(detail::build_backend != detail::Backend::Standard); \
  static void global_deallocate_raw(void* pointer, std::size_t alignment) noexcept;                                                        \
  static void* global_reallocate_raw(void* pointer, std::size_t old_bytes, std::size_t new_bytes,                                          \
                                     std::size_t alignment) noexcept(detail::build_backend != detail::Backend::Standard);                  \
                                                                                                                                           \
  Memory(MemoryKind kind) noexcept;                                                                                                        \
  void initialize(detail::BackendHandle handle, StatisticsMode statistics);                                                                \
  virtual detail::BackendHandle native_backend() const noexcept;                                                                           \
  void clear_allocation_statistics() noexcept;                                                                                             \
  void rewind_statistics(const void* buffer, std::size_t used) noexcept;                                                                   \
                                                                                                                                           \
  template <MemoryKind Kind, bool Zero>                                                                                                    \
  void* allocate_for(std::size_t bytes, std::size_t alignment);                                                                            \
  template <MemoryKind Kind>                                                                                                               \
  void* reallocate_for(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment);                                \
  template <MemoryKind Kind>                                                                                                               \
  void deallocate_for(void* pointer, std::size_t bytes, std::size_t alignment) noexcept;                                                   \
                                                                                                                                           \
  class Resource final : public std::pmr::memory_resource {                                                                                \
   public:                                                                                                                                 \
    Resource(Memory& memory) noexcept;                                                                                                     \
                                                                                                                                           \
   private:                                                                                                                                \
    void* do_allocate(std::size_t bytes, std::size_t alignment) override;                                                                  \
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;                                                  \
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;                                                      \
                                                                                                                                           \
    Memory& memory_;                                                                                                                       \
  };                                                                                                                                       \
                                                                                                                                           \
  const detail::BackendOps* ops_ = nullptr;                                                                                                \
  void* context_ = nullptr;                                                                                                                \
                                                                                                                                           \
  detail::TrackingContext* tracking_ = nullptr;                                                                                            \
                                                                                                                                           \
  const MemoryKind kind_;                                                                                                                  \
  Resource resource_;

#define UNIM_IMPL_GLOBAL_MEMBERS     \
  friend class Memory;               \
  Global(StatisticsMode statistics); \
  ~Global() override = default;

#define UNIM_IMPL_HEAP_MEMBERS                                                                                       \
  friend class Memory;                                                                                               \
  detail::BackendHandle native_backend() const noexcept override;                                                    \
  template <bool Zero>                                                                                               \
  void* allocate_raw(std::size_t bytes, std::size_t alignment) noexcept;                                             \
  void* reallocate_raw(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) noexcept; \
  void deallocate_raw(void* pointer, std::size_t bytes, std::size_t alignment) noexcept;                             \
  detail::BackendHandle heap_{};

#define UNIM_IMPL_STACK_MEMBERS                                                                                                              \
  friend class Memory;                                                                                                                       \
                                                                                                                                             \
  struct Storage {                                                                                                                           \
    std::byte* buffer;                                                                                                                       \
    std::size_t capacity;                                                                                                                    \
    std::size_t used = 0;                                                                                                                    \
    std::uint64_t generation = 0;                                                                                                            \
  };                                                                                                                                         \
                                                                                                                                             \
  static void* stack_allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept;                                             \
  static void* stack_reallocate(void* context, void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) noexcept; \
  static void stack_deallocate(void*, void*, std::size_t, std::size_t) noexcept;                                                             \
  static void stack_destroy(void*) noexcept;                                                                                                 \
                                                                                                                                             \
  Storage storage_;

#define UNIM_IMPL_OWNED_BLOCK_MEMBERS                                                           \
  friend class Memory;                                                                          \
  OwnedBlock(Memory& memory, void* pointer, std::size_t bytes, std::size_t alignment) noexcept; \
  void clear() noexcept;                                                                        \
                                                                                                \
  Memory* memory_ = nullptr;                                                                    \
  void* pointer_ = nullptr;                                                                     \
  std::size_t bytes_ = 0;                                                                       \
  std::size_t alignment_ = alignof(std::max_align_t);
