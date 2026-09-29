#include <unimem/memory.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <memory_resource>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif

namespace {
std::atomic<bool> fail_allocations{false};
void* raw_allocate(std::size_t bytes, std::size_t alignment) {
    if (fail_allocations.load()) { throw std::bad_alloc(); }
    bytes = bytes == 0 ? 1 : bytes;
    void* pointer = nullptr;
    if (alignment == 0) { pointer = std::malloc(bytes); }
    else {
#ifdef _WIN32
        pointer = _aligned_malloc(bytes, alignment);
#else
        if (posix_memalign(&pointer, alignment, bytes) != 0) { pointer = nullptr; }
#endif
    }
    if (!pointer) { throw std::bad_alloc(); }
    return pointer;
}
void release_aligned(void* pointer) noexcept {
#ifdef _WIN32
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
template<class F> bool fails(F&& function) {
    fail_allocations.store(true);
    try { function(); }
    catch (const std::bad_alloc&) { fail_allocations.store(false); return true; }
    catch (...) { fail_allocations.store(false); throw; }
    fail_allocations.store(false);
    return false;
}
}

// Isolated executable: the library's Standard backend sees real allocation failures.
void* operator new(std::size_t n) { return raw_allocate(n, 0); }
void* operator new[](std::size_t n) { return raw_allocate(n, 0); }
void* operator new(std::size_t n, std::align_val_t a) { return raw_allocate(n, std::size_t(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return raw_allocate(n, std::size_t(a)); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try { return raw_allocate(n, 0); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
    try { return raw_allocate(n, 0); } catch (...) { return nullptr; }
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return raw_allocate(n, std::size_t(a)); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return raw_allocate(n, std::size_t(a)); } catch (...) { return nullptr; }
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { release_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { release_aligned(p); }

int main(int argc, char** argv) {
    using namespace unimem;
    const auto mode = argc == 2 && std::strcmp(argv[1], "basic") == 0
        ? StatisticsMode::Basic : StatisticsMode::Disabled;
    Memory::configure_global(Backend::Standard, mode);
    if (mode == StatisticsMode::Basic && !fails([] { (void)Memory::global(); })) {
        return 1;
    }
    Memory& memory = Memory::global();
    fail_allocations.store(true);
    {
        auto empty = memory.make_block(0);
        if (empty.data() || empty.size() != 0) { return 5; }
        alignas(std::max_align_t) std::byte buffer[1024];
        Memory stack = Memory::stack(buffer);
        auto object = stack.make_unique<int>(42);
        auto shared = stack.make_shared<int>(17);
        std::pmr::vector<int> values(stack.resource());
        values.assign(16, 9);
        if (*object != 42 || *shared != 17 || values.back() != 9) { return 6; }
    }
    fail_allocations.store(false);
    {
        for (auto alignment : {std::size_t{1}, std::size_t{16}, std::size_t{4096}}) {
            auto block = memory.make_block(33, alignment);
            void* original = block.data();
            std::memset(original, 0x75, 33);
            for (unsigned i = 0; i < 100; ++i) {
                const auto before = memory.statistics();
                if (!fails([&] { memory.allocate(31, alignment); }) ||
                    !fails([&] { memory.allocate_zeroed(31, alignment); }) ||
                    !fails([&] { block.resize(65); }) ||
                    !fails([&] { memory.reallocate_zeroed(original, 33, 65, alignment); }) ||
                    !fails([&] { auto p = memory.make_shared<int>(42); })) { return 2; }
                if (block.data() != original || block.size() != 33) { return 3; }
                for (unsigned j = 0; j < 33; ++j) {
                    if (static_cast<unsigned char*>(original)[j] != 0x75) { return 4; }
                }
                const auto after = memory.statistics();
                if (before && (before->live_bytes != after->live_bytes ||
                               before->allocations != after->allocations ||
                               before->reallocations != after->reallocations)) { return 5; }
            }
        }
    }
    return 0;
}
