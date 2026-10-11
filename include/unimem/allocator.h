#pragma once

#include <unimem/common.h>

#include <cstddef>

namespace unimem {

template<class T>
class Allocator {
public:
    using value_type = T;

    explicit Allocator(Memory& memory) noexcept;

    template<class U>
    Allocator(const Allocator<U>& other) noexcept;

    T* allocate(std::size_t count);

    void deallocate(T* pointer, std::size_t count) noexcept;

    Memory& memory() const noexcept;

    template<class U>
    bool operator==(const Allocator<U>& other) const noexcept;

    template<class U>
    bool operator!=(const Allocator<U>& other) const noexcept;

private:
    Memory* memory_;
};

}

#include <unimem/detail/allocator.inl>
