#pragma once

#include <unimem/common.h>

#include <cstddef>
#include <memory>

namespace unimem {

template<class T>
struct Deleter {
    Memory* memory = nullptr;

    void operator()(T* pointer) const noexcept;
};

template<class T>
struct ArrayDeleter {
    Memory* memory = nullptr;
    std::size_t count = 0;

    void operator()(T* pointer) const noexcept;
};

template<class T> using Unique = std::unique_ptr<T, Deleter<T>>;
template<class T> using UniqueArray = std::unique_ptr<T[], ArrayDeleter<T>>;

}
