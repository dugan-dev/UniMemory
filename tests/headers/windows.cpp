#include <windows.h>
#include <unimem/memory.h>

int main() {
    auto& memory = unimem::Memory::global();
    auto owner = memory.make_unique<int>(42);
    auto block = memory.make_block(16);
    block.resize(32);
    return *owner == 42 && block.size() == 32 ? 0 : 1;
}
