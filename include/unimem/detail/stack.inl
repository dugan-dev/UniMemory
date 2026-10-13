#pragma once

namespace unimem {

inline detail::StackMark::StackMark(const Stack* owner, std::size_t used, std::uint64_t generation) noexcept
    : owner_(owner), used_(used), generation_(generation) {}

inline Stack::Mark Stack::mark() const {
  return Mark(this, storage_.used, storage_.generation);
}

inline void Stack::rewind(Mark mark) {
  auto& stack = storage_;
  if (mark.owner_ != this || mark.generation_ != stack.generation || mark.used_ > stack.used) {
    throw std::invalid_argument("UniMemory: invalid stack mark");
  }
  if (stack.generation == (std::numeric_limits<std::uint64_t>::max)()) {
    throw std::length_error("UniMemory: stack mark generation exhausted");
  }
  rewind_statistics(storage_.buffer, mark.used_);
  stack.used = mark.used_;
  ++stack.generation;
}

inline std::size_t Stack::used() const noexcept {
  return storage_.used;
}

inline std::size_t Stack::capacity() const noexcept {
  return storage_.capacity;
}

inline Stack::Stack(void* buffer, std::size_t capacity, StatisticsMode statistics)
    : Memory(MemoryKind::Stack), storage_{static_cast<std::byte*>(buffer), capacity} {
  memory_impl::validate_statistics(statistics);
  const auto address = reinterpret_cast<std::uintptr_t>(buffer);
  if ((buffer == nullptr && capacity != 0) || capacity > (std::numeric_limits<std::uintptr_t>::max)() - address) {
    throw std::invalid_argument("UniMemory: invalid stack buffer");
  }
  static const detail::BackendOps ops{stack_allocate, nullptr, stack_reallocate, stack_deallocate, stack_destroy};
  initialize({&ops, &storage_}, statistics);
}

inline Stack::Stack(std::span<std::byte> buffer, StatisticsMode statistics) : Stack(buffer.data(), buffer.size(), statistics) {}

UNIMEMORY_FORCE_INLINE void* Stack::stack_allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept {
  auto& stack = *static_cast<Storage*>(context);
  const auto address = reinterpret_cast<std::uintptr_t>(stack.buffer) + stack.used;
  const auto padding = (alignment - (address & (alignment - 1))) & (alignment - 1);
  if (padding > stack.capacity - stack.used || bytes > stack.capacity - stack.used - padding) {
    return nullptr;
  }
  stack.used += padding + bytes;
  return reinterpret_cast<void*>(address + padding);
}

UNIMEMORY_FORCE_INLINE void* Stack::stack_reallocate(void* context, void* pointer, std::size_t old_bytes, std::size_t new_bytes,
                                                     std::size_t alignment) noexcept {
  if (new_bytes <= old_bytes) {
    return pointer;
  }
  auto& stack = *static_cast<Storage*>(context);
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  const auto end = reinterpret_cast<std::uintptr_t>(stack.buffer) + stack.used;
  if (address <= end && old_bytes == end - address && new_bytes - old_bytes <= stack.capacity - stack.used) {
    stack.used += new_bytes - old_bytes;
    return pointer;
  }
  void* next = stack_allocate(context, new_bytes, alignment);
  if (next != nullptr) {
    std::memcpy(next, pointer, old_bytes);
  }
  return next;
}

UNIMEMORY_FORCE_INLINE void Stack::stack_deallocate(void*, void*, std::size_t, std::size_t) noexcept {}

inline void Stack::stack_destroy(void*) noexcept {}

inline void Stack::reset() {
  if (storage_.generation == (std::numeric_limits<std::uint64_t>::max)()) {
    throw std::length_error("UniMemory: stack mark generation exhausted");
  }
  storage_.used = 0;
  ++storage_.generation;
  clear_allocation_statistics();
}

}  // namespace unimem
