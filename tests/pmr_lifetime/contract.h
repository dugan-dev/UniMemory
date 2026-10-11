#pragma once

#include <unimem/memory.h>

#include <cstddef>
#include <memory_resource>

struct PmrIdentity {
    unimem::Memory* memory;
    std::pmr::memory_resource* resource;
};
PmrIdentity pmr_identity_a();
PmrIdentity pmr_identity_b();
void* pmr_allocate_a(std::size_t bytes, std::size_t alignment);
void pmr_free_b(void* pointer, std::size_t bytes, std::size_t alignment);
void pmr_arm_late_call();
