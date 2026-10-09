#pragma once

#include <unimem/common.h>

#include <cstddef>

namespace unimem {

template<class T>
class Allocator {
public:
    using value_type = T;

    explicit Allocator(Memory& memory) noexcept : memory_(&memory) {}

    template<class U>
    Allocator(const Allocator<U>& other) noexcept : memory_(&other.memory()) {}

    T* allocate(std::size_t count);

    void deallocate(T* pointer, std::size_t count) noexcept;

    Memory& memory() const noexcept { return *memory_; }

    template<class U>
    bool operator==(const Allocator<U>& other) const noexcept {
        return memory_ == &other.memory();
    }

    template<class U>
    bool operator!=(const Allocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    Memory* memory_;
};

}

// Complete Memory and the adapter templates after declaring Allocator.
#include <unimem/memory.h>
