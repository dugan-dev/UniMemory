#include <unimem/memory.h>

#include <cstdint>
#include <utility>

bool header_owned_block() {
    unimem::Memory& memory = unimem::Memory::global();
    unimem::OwnedBlock block = memory.make_block(33, 64);
    static_cast<std::byte*>(block.data())[0] = std::byte{42};
    unimem::OwnedBlock moved = std::move(block);
    moved.resize(65);
    bool valid = block.data() == nullptr && block.size() == 0 &&
                 moved.size() == 65 && moved.alignment() == 64 &&
                 reinterpret_cast<std::uintptr_t>(moved.data()) % 64 == 0 &&
                 static_cast<std::byte*>(moved.data())[0] == std::byte{42};
    moved.resize(0);
    return valid && moved.data() == nullptr && moved.size() == 0;
}
