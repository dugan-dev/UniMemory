#include <unimem/memory.h>
#include <mimalloc.h>
#include <cstring>
#include <new>

namespace { bool fail_heap_creation = false; }
extern "C" mi_heap_t* __real_mi_heap_new();
extern "C" mi_heap_t* __wrap_mi_heap_new() {
    return fail_heap_creation ? nullptr : __real_mi_heap_new();
}

// GNU linker interception affects this executable only, not library behavior.
int main() {
    using namespace unimem;
    for (auto mode : {StatisticsMode::Disabled, StatisticsMode::Basic}) {
        Memory arena = Memory::heap(Backend::Mimalloc, mode);
        for (unsigned round = 0; round < 128; ++round) {
            auto block = arena.make_block(257, 256);
            std::memset(block.data(), 31, block.size());
            const auto before = arena.statistics();
            fail_heap_creation = true;
            bool caught = false;
            try { arena.reset(); }
            catch (const std::bad_alloc&) { caught = true; }
            fail_heap_creation = false;
            if (!caught || !arena.owns(block.data()) || block.size() != 257) { return 1; }
            for (unsigned i = 0; i < 257; ++i) {
                if (static_cast<unsigned char*>(block.data())[i] != 31) { return 2; }
            }
            const auto after = arena.statistics();
            if (before && (after->allocations != before->allocations ||
                           after->live_bytes != before->live_bytes ||
                           after->peak_live_bytes != before->peak_live_bytes)) { return 3; }
            block.resize(0);
            arena.reset();
        }
    }
    return 0;
}
