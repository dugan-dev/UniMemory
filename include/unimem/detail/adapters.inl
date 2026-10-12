#pragma once

namespace unimem {

template <class T>
inline Allocator<T>::Allocator(Memory& memory) noexcept : memory_(&memory) {}

template <class T>
template <class U>
inline Allocator<T>::Allocator(const Allocator<U>& other) noexcept : memory_(&other.memory()) {}

template <class T>
inline Memory& Allocator<T>::memory() const noexcept {
  return *memory_;
}

template <class T>
template <class U>
inline bool Allocator<T>::operator==(const Allocator<U>& other) const noexcept {
  return memory_ == &other.memory();
}

template <class T>
template <class U>
inline bool Allocator<T>::operator!=(const Allocator<U>& other) const noexcept {
  return !(*this == other);
}

inline void* OwnedBlock::data() const noexcept {
  return pointer_;
}

inline std::size_t OwnedBlock::size() const noexcept {
  return bytes_;
}

inline std::size_t OwnedBlock::alignment() const noexcept {
  return alignment_;
}

UNIMEMORY_FORCE_INLINE std::pmr::memory_resource* Memory::resource() noexcept {
  return &resource_;
}

template <class T>
UNIMEMORY_FORCE_INLINE Allocator<T> Memory::allocator() noexcept {
  return Allocator<T>(*this);
}

inline Memory::Resource::Resource(Memory& memory) noexcept : memory_(memory) {}

UNIMEMORY_FORCE_INLINE void* Memory::Resource::do_allocate(std::size_t bytes, std::size_t alignment) {
  return memory_.allocate(bytes == 0 ? 1 : bytes, alignment);
}

UNIMEMORY_FORCE_INLINE void Memory::Resource::do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) {
  memory_.deallocate(pointer, bytes == 0 ? 1 : bytes, alignment);
}

inline bool Memory::Resource::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
  return this == &other;
}

template <class T>
UNIMEMORY_FORCE_INLINE T* Allocator<T>::allocate(std::size_t count) {
  if (count == 0) {
    return static_cast<T*>(memory_->allocate(1, alignof(T)));
  }
  return detail::allocate_array_storage<T>(*memory_, count);
}

template <class T>
UNIMEMORY_FORCE_INLINE void Allocator<T>::deallocate(T* pointer, std::size_t count) noexcept {
  memory_->deallocate(const_cast<std::remove_cv_t<T>*>(pointer), count == 0 ? 1 : count * sizeof(T), alignof(T));
}

template <class T>
UNIMEMORY_FORCE_INLINE void Deleter<T>::operator()(T* pointer) const noexcept {
  if (pointer == nullptr) {
    return;
  }
#if UNIMEMORY_CHECKS
  if (memory == nullptr) {
    assert(memory != nullptr && "UniMemory: nonempty Unique requires a bound Memory");
    std::terminate();
  }
#endif
  memory->destroy(pointer);
}

template <class T>
UNIMEMORY_FORCE_INLINE void ArrayDeleter<T>::operator()(T* pointer) const noexcept {
  if (pointer == nullptr) {
    return;
  }
#if UNIMEMORY_CHECKS
  if (memory == nullptr || count == 0) {
    assert(memory != nullptr && count != 0 && "UniMemory: nonempty UniqueArray requires its Memory and count");
    std::terminate();
  }
#endif
  memory->destroy_array(pointer, count);
}

UNIMEMORY_FORCE_INLINE OwnedBlock Memory::make_block(std::size_t bytes, std::size_t alignment) {
  return OwnedBlock(*this, allocate(bytes, alignment), bytes, alignment);
}

UNIMEMORY_FORCE_INLINE OwnedBlock::OwnedBlock(Memory& memory, void* pointer, std::size_t bytes, std::size_t alignment) noexcept
    : memory_(&memory), pointer_(pointer), bytes_(bytes), alignment_(alignment) {}

UNIMEMORY_FORCE_INLINE OwnedBlock::~OwnedBlock() {
  clear();
}

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
  if (memory_ == nullptr) {
    throw std::logic_error("UniMemory: moved-from block");
  }
  void* next = memory_->reallocate(pointer_, bytes_, new_bytes, alignment_);
  pointer_ = next;
  bytes_ = new_bytes;
}

UNIMEMORY_FORCE_INLINE void OwnedBlock::clear() noexcept {
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

}  // namespace unimem
