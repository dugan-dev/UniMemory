#include "compiled-test-config.h"
#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace unimem;

static_assert(!std::is_default_constructible_v<Memory>);
static_assert(!std::is_constructible_v<Memory, Backend>);
static_assert(!std::is_copy_constructible_v<Memory>);
static_assert(!std::is_move_constructible_v<Memory>);
static_assert(!std::is_polymorphic_v<Memory>);

namespace {
void check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
template<class E, class F> void throws(F&& function) {
    try { function(); } catch (const E&) { return; }
    throw std::runtime_error("expected exception missing");
}
Backend parse(const std::string& name) {
    if (name == "standard") { return Backend::Standard; }
    if (name == "mimalloc") { return Backend::Mimalloc; }
    if (name == "jemalloc") { return Backend::Jemalloc; }
    throw std::invalid_argument("unknown backend");
}

struct alignas(256) Object {
    static inline unsigned alive = 0, attempts = 0, fail_at = 0;
    int value = 42;
    Object() {
        if (++attempts == fail_at) { throw std::runtime_error("constructor failure"); }
        ++alive;
    }
    ~Object() noexcept { --alive; }
};

bool shutdown_enabled = false;
Backend shutdown_backend = Backend::Standard;
struct ShutdownProbe {
    ~ShutdownProbe() noexcept {
        if (!shutdown_enabled) { return; }
        try {
            Memory& memory = Memory::global(shutdown_backend);
            auto object = memory.make_unique<Object>();
            auto block = memory.make_block(128, 64);
            if (object->value != 42 || block.size() != 128) { std::abort(); }
        } catch (...) { std::abort(); }
    }
} shutdown_probe;

void global_test(Backend backend, StatisticsMode mode) {
    compiled_test::test_configuration(backend, compiled_test::global_mode);
    std::array<Memory*, 16> addresses{};
    std::array<std::thread, 16> workers;
    for (std::size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::thread([&, i] { addresses[i] = &Memory::global(backend); });
    }
    for (auto& worker : workers) { worker.join(); }
    Memory& memory = *addresses[0];
    for (Memory* address : addresses) { check(address == &memory, "duplicate Global Memory"); }
    check(memory.kind() == MemoryKind::Global && memory.backend() == backend, "global identity");
    const auto caps = memory.capabilities();
    check(caps.basic_statistics == compiled_test::supports_statistics && caps.thread_safe && caps.individual_reclaim &&
          !caps.reset && !caps.collect && !caps.owns && !caps.checkpoints, "global capabilities");
    check(caps.detailed_statistics == capabilities(backend).detailed_statistics, "native capability");
    check(memory.statistics().has_value() == (mode == StatisticsMode::Basic), "statistics mode");
    compiled_test::verify_configuration();
    throws<std::logic_error>([&] { memory.reset(); });
    throws<std::logic_error>([&] { memory.collect(); });
    throws<std::logic_error>([&] { memory.owns(nullptr); });
    throws<std::logic_error>([&] { memory.mark(); });
    throws<std::logic_error>([&] { memory.used(); });
    throws<std::logic_error>([&] { memory.capacity(); });
    std::byte buffer[16];
    Memory scratch = Memory::stack(buffer);
    throws<std::logic_error>([&] { memory.rewind(scratch.mark()); });
    throws<std::invalid_argument>([] { Memory::global(static_cast<Backend>(99)); });
    for (auto candidate : {Backend::Mimalloc, Backend::Jemalloc}) {
        if (!available(candidate)) {
            throws<std::invalid_argument>([&] { Memory::global(candidate); });
            throws<std::invalid_argument>([&] { Memory::heap(candidate); });
        }
    }

    const auto before = memory.statistics();
    for (unsigned i = 0; i < 4096; ++i) {
        Memory& first = Memory::global(backend);
        Memory& second = Memory::global(backend);
        auto* pointer = first.allocate_objects<unsigned>();
        *pointer = i;
        check(*pointer == i, "global content");
        second.deallocate_objects(pointer);
    }
    std::atomic<bool> failed{false};
    for (auto& worker : workers) {
        worker = std::thread([&] {
            try {
                for (unsigned i = 0; i < 2048; ++i) {
                    auto owner = Memory::global(backend).make_unique<unsigned>(i);
                    if (*owner != i) { failed.store(true); }
                }
            } catch (...) { failed.store(true); }
        });
    }
    for (auto& worker : workers) { worker.join(); }
    check(!failed.load(), "global worker failed");
    if (before) {
        const auto after = memory.statistics();
        check( (!compiled_test::supports_statistics || (after->live_bytes == 0)) && (!compiled_test::supports_statistics || (after->allocations - before->allocations == 36864)) && (!compiled_test::supports_statistics || (after->deallocations - before->deallocations == 36864)) , "global accounting");
    }
    {
        auto object = memory.make_unique<Object>();
        auto shared = memory.make_shared<Object>();
        std::vector<unsigned, Allocator<unsigned>> ordinary(memory.allocator<unsigned>());
        std::pmr::vector<std::pmr::string> nested(memory.resource());
        ordinary.assign(1024, 7);
        nested.emplace_back(128, 'x');
        check(object->value == 42 && shared->value == 42 && ordinary.back() == 7 &&
              nested.front().get_allocator().resource() == memory.resource(), "global adapters");
    }
    check(Object::alive == 0, "global Object lifetime");
    if (capabilities(backend).heap) {
        Memory heap = Memory::heap(backend, mode);
        check(heap.kind() == MemoryKind::Heap && heap.backend() == backend, "heap identity");
        const auto cap = heap.capabilities();
        check(cap.reset && cap.collect && cap.owns && !cap.checkpoints, "heap capabilities");
        throws<std::logic_error>([&] { heap.mark(); });
        throws<std::logic_error>([&] { heap.used(); });
        throws<std::logic_error>([&] { heap.capacity(); });
        throws<std::logic_error>([&] { heap.rewind(scratch.mark()); });
        {
            auto object = heap.make_unique<Object>();
            check(heap.owns(object.get()) && !heap.owns(nullptr), "heap ownership");
            heap.collect();
            check(object->value == 42, "heap collection invalidated Object");
        }
        heap.reset();
        if (mode == StatisticsMode::Basic) {
            check( (!compiled_test::supports_statistics || (heap.statistics()->allocations == 0)) , "heap statistics epoch");
        }
    } else {
        throws<std::runtime_error>([&] { Memory::heap(backend); });
    }
    shutdown_backend = backend;
    shutdown_enabled = true;
}

void stack_boundaries() {
    Memory empty = Memory::stack(nullptr, 0);
    check(empty.allocate(0) == nullptr && empty.capacity() == 0, "empty Stack");
    throws<std::bad_alloc>([&] { empty.allocate(1); });
    throws<std::invalid_argument>([] { Memory::stack(nullptr, 1); });
    std::byte buffer[64];
    throws<std::invalid_argument>([&] { Memory::stack(buffer, std::numeric_limits<std::size_t>::max()); });
    Memory stack = Memory::stack(buffer);
    check(stack.kind() == MemoryKind::Stack && !stack.backend(), "Stack must not invent a Backend");
    const auto caps = stack.capabilities();
    check(caps.reset && caps.checkpoints && !caps.collect && !caps.owns &&
          !caps.thread_safe && !caps.individual_reclaim && !caps.basic_statistics &&
          !caps.detailed_statistics, "Stack capabilities");
    check(!stack.statistics() && !stack.backend_statistics(), "Stack statistics");
    throws<std::logic_error>([&] { stack.collect(); });
    throws<std::logic_error>([&] { stack.owns(nullptr); });
#if UNIMEMORY_CHECKS
    throws<std::invalid_argument>([&] { stack.allocate(0, 0); });
    throws<std::invalid_argument>([&] { stack.allocate(1, 3); });
#endif
    const auto original = stack.mark();
    auto block = stack.make_block(16, 1);
    void* pointer = block.data();
    std::memset(pointer, 0x5A, 16);
    block.resize(32);
    check(block.data() == pointer && stack.used() == 32, "tail growth");
    block.resize(8);
    check(block.data() == pointer && stack.used() == 32, "shrink must not reclaim");
    const auto used = stack.used();
    throws<std::bad_alloc>([&] { block.resize(std::numeric_limits<std::size_t>::max()); });
    check(block.data() == pointer && block.size() == 8 && stack.used() == used,
          "failed growth mutated Stack");
    check(static_cast<unsigned char*>(pointer)[7] == 0x5A, "failed growth lost bytes");
    block.resize(0);
    check(stack.used() == used, "individual release reclaimed Stack");
    stack.rewind(original);
    check(stack.used() == 0, "rewind failed");
    throws<std::invalid_argument>([&] { stack.rewind(original); });
    auto stale = stack.mark();
    stack.reset();
    throws<std::invalid_argument>([&] { stack.rewind(stale); });

    auto* first = static_cast<unsigned char*>(stack.allocate(8, 1));
    std::memset(first, 0x3A, 8);
    auto* guard = static_cast<unsigned char*>(stack.allocate(8, 1));
    std::memset(guard, 0xC7, 8);
    auto* next = static_cast<unsigned char*>(stack.reallocate_zeroed(first, 8, 20, 1));
    check(next != first && guard[0] == 0xC7, "non-tail resize overwritten neighbor");
    for (unsigned i = 0; i < 20; ++i) {
        check(next[i] == (i < 8 ? 0x3A : 0), "Stack zeroed resize");
    }
    throws<std::length_error>([&] { stack.allocate_objects<unsigned>(std::numeric_limits<std::size_t>::max()); });
}

void stack_matrix(unsigned index) {
    check(index < 240, "invalid matrix case");
    constexpr std::array<std::size_t, 8> sizes{0, 1, 7, 16, 64, 512, 4096, 8192};
    constexpr std::array<std::size_t, 5> alignments{1, 8, 16, 64, 4096};
    const auto bytes = sizes[index % 8], alignment = alignments[index / 8 % 5];
    const auto path = index / 40;
    std::vector<std::byte> backing(262144);
    Memory stack = Memory::stack(backing);
    Memory other = Memory::stack(nullptr, 0);
    for (unsigned cycle = 0; cycle < 64; ++cycle) {
        const auto checkpoint = stack.mark();
        throws<std::invalid_argument>([&] { stack.rewind(other.mark()); });
        unsigned char* guard = nullptr;
        if (path >= 3 && bytes != 0) {
            guard = static_cast<unsigned char*>(stack.allocate(bytes, alignment));
            check(reinterpret_cast<std::uintptr_t>(guard) % alignment == 0, "adapter guard alignment");
            std::memset(guard, 0xC9, bytes);
        }
        if (path == 4) {
            {
                auto object = stack.make_unique<Object>();
                auto shared = stack.make_shared<Object>();
                auto array = stack.make_unique_array<Object>(1 + bytes % 64);
                std::weak_ptr<Object> weak = shared;
                shared.reset();
                check(weak.expired(), "Stack shared lifetime");
                weak.reset();
                check(object->value == 42 && array[0].value == 42, "Stack Object adapters");
            }
            check(Object::alive == 0, "Stack Owner did not destroy Object");
            Object::attempts = 0;
            Object::fail_at = 3;
            throws<std::runtime_error>([&] { stack.create_array<Object>(5); });
            Object::fail_at = 0;
            check(Object::alive == 0, "Stack Array exception rollback");
        } else if (path == 5) {
            const auto count = 32 + bytes % 128;
            {
                std::vector<unsigned, Allocator<unsigned>> values(stack.allocator<unsigned>());
                std::pmr::vector<std::pmr::string> nested(stack.resource());
                values.assign(count, cycle);
                nested.emplace_back(128, 'x');
                check(values.back() == cycle && nested.front().get_allocator().resource() ==
                      stack.resource(), "Stack Container adapters");
            }
            const auto retained = stack.used();
            check(retained != 0, "Container release must retain Stack space");
        } else if (path == 3) {
            const auto count = bytes;
            std::byte* typed = stack.allocate_objects<std::byte>(count);
            if (count != 0) { std::fill_n(typed, count, std::byte{0x6B}); }
            if (count != 0) { check(typed[count - 1] == std::byte{0x6B}, "typed Stack storage"); }
            stack.deallocate_objects(typed, count);
            struct alignas(4096) AlignedObject { unsigned value = 83; };
            {
                auto object = stack.make_unique<AlignedObject>();
                check(reinterpret_cast<std::uintptr_t>(object.get()) % 4096 == 0 &&
                      object->value == 83, "over-aligned Stack Object");
            }
        } else {
            std::array<void*, 8> pointers{};
            for (auto& pointer : pointers) {
                pointer = path == 1 ? stack.allocate_zeroed(bytes, alignment)
                                    : stack.allocate(bytes, alignment);
                if (bytes == 0) { check(pointer == nullptr, "Stack zero allocation"); continue; }
                check(reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0, "Stack alignment");
                if (path == 1) {
                    check(std::all_of(static_cast<unsigned char*>(pointer),
                        static_cast<unsigned char*>(pointer) + bytes,
                        [](unsigned char byte) { return byte == 0; }), "Stack zeroing");
                }
                std::memset(pointer, 0x61, bytes);
            }
            for (auto pointer : pointers) {
                const auto grown = bytes + 17;
                auto* next = static_cast<unsigned char*>(stack.reallocate_zeroed(pointer, bytes, grown, alignment));
                for (std::size_t i = 0; i < grown; ++i) {
                    check(next[i] == (i < bytes ? 0x61 : 0), "Stack resize content");
                }
                stack.deallocate(next, grown, alignment);
            }
            if (path == 2) {
                auto block = stack.make_block(bytes, alignment);
                block.resize(bytes + 33);
                auto moved = std::move(block);
                check(!block.data() && moved.size() == bytes + 33, "Stack OwnedBlock move");
                throws<std::logic_error>([&] { block.resize(1); });
            }
        }
        if (guard != nullptr) {
            check(guard[0] == 0xC9 && guard[bytes - 1] == 0xC9, "adapter overwrote guard");
            stack.deallocate(guard, bytes, alignment);
        }
        stack.rewind(checkpoint);
        check(stack.used() == 0, "Stack cycle did not recover capacity");
        throws<std::invalid_argument>([&] { stack.rewind(checkpoint); });
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "stack") {
            if (std::string(argv[2]) == "boundary") { stack_boundaries(); }
            else { stack_matrix(static_cast<unsigned>(std::stoul(argv[2]))); }
        } else if (argc == 3) {
            global_test(parse(argv[1]), std::string(argv[2]) == "basic"
                ? StatisticsMode::Basic : StatisticsMode::Disabled);
        } else { throw std::invalid_argument("backend mode or stack case required"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "unified Memory: " << error.what() << '\n';
        return 1;
    }
}
