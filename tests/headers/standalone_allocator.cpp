#include <unimem/allocator.h>

#include <memory>
#include <vector>

namespace {
struct Value { int value; };
}

bool standalone_allocator(unimem::Memory& memory) {
    unimem::Allocator<Value> allocator(memory);
    Value* storage = allocator.allocate(3);
    for (int index = 0; index < 3; ++index) {
        std::construct_at(storage + index, index + 7);
    }
    const bool valid = storage[0].value == 7 && storage[2].value == 9;
    std::destroy_n(storage, 3);
    allocator.deallocate(storage, 3);
    std::vector<Value, unimem::Allocator<Value>> values(allocator);
    values.push_back({11});
    return valid && values.back().value == 11;
}
