#pragma once

#include <unimem/common.h>

#include <cstddef>

namespace unimem {

class OwnedBlock final {
public:
    ~OwnedBlock();
    OwnedBlock(const OwnedBlock&) = delete;
    OwnedBlock& operator=(const OwnedBlock&) = delete;
    OwnedBlock(OwnedBlock&& other) noexcept;
    OwnedBlock& operator=(OwnedBlock&& other) noexcept;

    void* data() const noexcept { return pointer_; }
    std::size_t size() const noexcept { return bytes_; }
    std::size_t alignment() const noexcept { return alignment_; }

    void resize(std::size_t new_bytes);

private:
    friend class Memory;
    OwnedBlock(Memory& memory, void* pointer, std::size_t bytes,
               std::size_t alignment) noexcept;
    void clear() noexcept;

    Memory* memory_ = nullptr;
    void* pointer_ = nullptr;
    std::size_t bytes_ = 0;
    std::size_t alignment_ = alignof(std::max_align_t);
};

}
