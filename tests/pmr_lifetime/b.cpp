#include "contract.h"

PmrIdentity pmr_identity_b() {
  auto& memory = pmr_context();
  return {&memory, memory.resource()};
}
void pmr_free_b(void* pointer, std::size_t bytes, std::size_t alignment) {
  pmr_context().resource()->deallocate(pointer, bytes, alignment);
}
