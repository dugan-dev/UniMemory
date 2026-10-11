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

    void* data() const noexcept;
    std::size_t size() const noexcept;
    std::size_t alignment() const noexcept;

    void resize(std::size_t new_bytes);

private:
#include <unimem/detail/owned_block_members.inl>
};

}

#include <unimem/memory.h>
