#include "contract.h"

PmrIdentity pmr_identity_b() {
    auto& memory = unimem::Memory::global(unimem::Memory::selected_backend);
    return {&memory, memory.resource()};
}
void pmr_free_b(void* pointer, std::size_t bytes, std::size_t alignment) {
    unimem::Memory::global().resource()->deallocate(pointer, bytes, alignment);
}
