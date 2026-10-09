#if UNIMEMORY_HEADER_ORDER == 0
#include <unimem/allocator.h>
#include <unimem/smart_ptr.h>
#include <unimem/memory.h>
#elif UNIMEMORY_HEADER_ORDER == 1
#include <unimem/allocator.h>
#include <unimem/memory.h>
#include <unimem/smart_ptr.h>
#elif UNIMEMORY_HEADER_ORDER == 2
#include <unimem/smart_ptr.h>
#include <unimem/allocator.h>
#include <unimem/memory.h>
#elif UNIMEMORY_HEADER_ORDER == 3
#include <unimem/smart_ptr.h>
#include <unimem/memory.h>
#include <unimem/allocator.h>
#elif UNIMEMORY_HEADER_ORDER == 4
#include <unimem/memory.h>
#include <unimem/allocator.h>
#include <unimem/smart_ptr.h>
#else
#include <unimem/memory.h>
#include <unimem/smart_ptr.h>
#include <unimem/allocator.h>
#endif

int main() {
    auto& memory = unimem::Memory::global();
    auto allocator = memory.allocator<int>();
    auto* storage = allocator.allocate(2);
    std::construct_at(storage, 17);
    std::construct_at(storage + 1, 23);
    auto owner = memory.adopt_unique_array(storage, 2);
    return owner[0] == 17 && owner[1] == 23 ? 0 : 1;
}
