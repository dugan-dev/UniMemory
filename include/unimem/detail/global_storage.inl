#pragma once

#include <cstddef>

// Included after Memory is complete. Trivial bytes provide static storage;
// no Memory/SDK initialization or destruction occurs at namespace scope.
namespace unimem::detail {
struct alignas(Memory) GlobalStorage { std::byte bytes[sizeof(Memory)]; };
static_assert(alignof(GlobalStorage) >= alignof(Memory));
inline GlobalStorage global_storage{};
}
