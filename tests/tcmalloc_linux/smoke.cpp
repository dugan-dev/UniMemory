#include <unimem/memory.h>

#include <cstddef>
#include <memory_resource>
#include <vector>

int main() {
    unimem::Memory& memory = unimem::Memory::global();
    auto object = memory.make_unique<int>(42);
    std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
    values.push_back(*object);
    std::pmr::vector<int> polymorphic(memory.resource());
    polymorphic.push_back(values[0]);
    void* bytes = memory.allocate(128, 64);
    memory.deallocate(bytes, 128, 64);
    return polymorphic[0] == 42 ? 0 : 1;
}
