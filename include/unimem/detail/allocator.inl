#pragma once

namespace unimem {

template<class T>
inline Allocator<T>::Allocator(Memory& memory) noexcept : memory_(&memory) {}

template<class T>
template<class U>
inline Allocator<T>::Allocator(const Allocator<U>& other) noexcept
    : memory_(&other.memory()) {}

template<class T>
inline Memory& Allocator<T>::memory() const noexcept { return *memory_; }

template<class T>
template<class U>
inline bool Allocator<T>::operator==(const Allocator<U>& other) const noexcept {
    return memory_ == &other.memory();
}

template<class T>
template<class U>
inline bool Allocator<T>::operator!=(const Allocator<U>& other) const noexcept {
    return !(*this == other);
}

}
