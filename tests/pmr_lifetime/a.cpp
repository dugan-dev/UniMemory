#include "contract.h"

#include <algorithm>
#include <cstdlib>

namespace {
// Created before main, armed after Global publication, used after main.
struct LatePmr {
    unimem::Memory* memory = nullptr;
    std::pmr::memory_resource* resource = nullptr;
    ~LatePmr() {
        if (!resource) { return; }
        try {
            const auto before = memory->statistics();
            void* pointer = resource->allocate(3, 16);
            resource->deallocate(pointer, 3, 16);
            const auto after = memory->statistics();
            if (before && (!after || after->allocations != before->allocations + 1 ||
                           after->deallocations != before->deallocations + 1 ||
                           after->reallocations != before->reallocations || after->live_bytes != before->live_bytes ||
                           after->peak_live_bytes != std::max(before->peak_live_bytes, before->live_bytes + 3))) {
                std::_Exit(79);
            }
        } catch (...) { std::_Exit(78); }
    }
} late_pmr;
}

PmrIdentity pmr_identity_a() {
    auto& memory = unimem::Memory::global();
    return {&memory, memory.resource()};
}
void* pmr_allocate_a(std::size_t bytes, std::size_t alignment) {
    return unimem::Memory::global().resource()->allocate(bytes, alignment);
}
void pmr_arm_late_call() {
    const auto identity = pmr_identity_a();
    late_pmr.memory = identity.memory;
    late_pmr.resource = identity.resource;
}
