#include <unimem/memory.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

template<class Exception, class F>
void check_throws(F&& function, const char* message) {
    try { function(); }
    catch (const Exception&) { return; }
    throw std::runtime_error(message);
}

unimem::Backend parse_backend(const std::string& name) {
    if (name == "standard") { return unimem::Backend::Standard; }
    if (name == "mimalloc") { return unimem::Backend::Mimalloc; }
    if (name == "jemalloc") { return unimem::Backend::Jemalloc; }
    throw std::invalid_argument("unknown backend");
}

bool aligned(const void* pointer, std::size_t alignment) {
    return reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

struct alignas(256) WideObject { int value; };

struct ThrowingObject {
    ThrowingObject() { throw std::runtime_error("constructor"); }
};

void smoke(unimem::Backend backend) {
    const auto cap = unimem::capabilities(backend);
    check(cap.available && cap.heap == (backend != unimem::Backend::Standard),
          "backend capability");
    check(cap.release_delay == unimem::supports(
        backend, unimem::RuntimeOption::UnusedPageReleaseDelayMs),
        "runtime capability");

    unimem::Memory& memory = unimem::Memory::global(backend);
    auto* object = memory.create<WideObject>();
    check(aligned(object, alignof(WideObject)), "object alignment");
    object->value = 91;
    check(object->value == 91, "object value");
    memory.destroy(object);
    auto unique = memory.make_unique<int>(17);
    check(*unique == 17, "unique object");
    unique.reset();
    auto array = memory.make_unique_array<int>(33);
    for (std::size_t i = 0; i < 33; ++i) { array[i] = static_cast<int>(i); }
    check(array[32] == 32, "unique array");
    array.reset();
    auto shared = memory.make_shared<int>(23);
    check(*shared == 23, "shared object");
    shared.reset();

    {
        std::vector<int, unimem::Allocator<int>> ordinary(memory.allocator<int>());
        std::pmr::vector<int> polymorphic(memory.resource());
        for (int i = 0; i < 100; ++i) {
            ordinary.push_back(i);
            polymorphic.push_back(i * 2);
        }
        check(ordinary[99] == 99 && polymorphic[99] == 198,
              "container content");
    }

    auto stats = memory.statistics();
    check(stats && stats->live_bytes == 0 && stats->allocations > 0,
          "request statistics");
    const auto detail = memory.backend_statistics();
    check(detail.has_value() == cap.detailed_statistics,
          "detailed statistics capability");
    if (detail) {
        check(detail->allocated_bytes || detail->committed_bytes ||
              detail->reserved_bytes || detail->resident_bytes, "available native metric");
        check(detail->scope == unimem::BackendStatisticsScope::Process,
              "metric scope");
    }

    if (cap.heap) {
        unimem::Memory arena = unimem::Memory::heap(backend, unimem::StatisticsMode::Basic);
        auto& scoped = arena;
        auto block = scoped.make_block(256, 64);
        check(block.size() == 256 && aligned(block.data(), 64), "arena block");
        auto inside = scoped.make_unique<int>(29);
        check(*inside == 29, "arena object");
        inside.reset();
        block.resize(0);
        check(scoped.statistics()->live_bytes == 0, "arena lifetime");
        const auto arena_detail = scoped.backend_statistics();
        if (arena_detail) {
            check(arena_detail->scope == unimem::BackendStatisticsScope::Memory,
                  "arena statistics scope");
        }

    } else {
        check_throws<std::runtime_error>([&] { unimem::Memory unsupported = unimem::Memory::heap(backend); },
                                        "unsupported arena must throw");
    }

    if (cap.release_delay) {
        check(unimem::set_runtime_option(
                  backend, unimem::RuntimeOption::UnusedPageReleaseDelayMs, 10),
              "set release delay");
        check_throws<std::invalid_argument>([&] {
            unimem::set_runtime_option(
                backend, unimem::RuntimeOption::UnusedPageReleaseDelayMs, -2);
        }, "invalid release delay");
    } else {
        check(!unimem::set_runtime_option(
                  backend, unimem::RuntimeOption::UnusedPageReleaseDelayMs, 10),
              "unsupported release delay");
    }
}

void boundaries(unimem::Backend backend) {
    unimem::Memory& memory = unimem::Memory::global(backend);
    check(memory.allocate(0) == nullptr, "zero size");
    check_throws<std::invalid_argument>([&] { memory.allocate(4, 3); },
                                        "invalid alignment");
    check_throws<std::invalid_argument>([&] { memory.allocate(0, 0); },
                                        "zero alignment");
    check_throws<std::length_error>([&] {
        memory.allocate_objects<std::uint64_t>(
            std::numeric_limits<std::size_t>::max());
    }, "object count overflow");
    check_throws<std::runtime_error>([&] { memory.create<ThrowingObject>(); },
                                     "constructor exception");
    check(memory.statistics()->live_bytes == 0, "constructor rollback");

    auto block = memory.make_block(17, 256);
    std::memset(block.data(), 0x5A, block.size());
    try { block.resize(std::numeric_limits<std::size_t>::max()); }
    catch (const std::bad_alloc&) {}
    check(block.size() == 17 && aligned(block.data(), 256),
          "failed resize retained block");
    const auto* bytes = static_cast<const unsigned char*>(block.data());
    for (std::size_t i = 0; i < 17; ++i) {
        check(bytes[i] == 0x5A, "failed resize retained bytes");
    }
    block.resize(0);
    check(memory.statistics()->live_bytes == 0, "all bytes released");

    if (!unimem::capabilities(backend).heap) { return; }
    unimem::Memory arena = unimem::Memory::heap(backend);
    auto* pointer = arena.allocate_zeroed(33, 64);
    for (std::size_t i = 0; i < 33; ++i) {
        check(static_cast<unsigned char*>(pointer)[i] == 0, "arena zeroing");
    }
    arena.deallocate(pointer, 33, 64);
}

void matrix(unimem::Backend backend, unsigned index) {
    check(index < 240, "case index");
    constexpr std::array<std::size_t, 8> sizes{
        0, 1, 7, 16, 64, 512, 4096, 65536};
    constexpr std::array<std::size_t, 5> alignments{1, 8, 16, 64, 256};
    const auto size = sizes[index % sizes.size()];
    const auto alignment = alignments[(index / sizes.size()) % alignments.size()];
    const auto path = index / (sizes.size() * alignments.size());
    const bool use_arena = path >= 3 && unimem::capabilities(backend).heap;
    const bool zeroed = path == 1 || path == 4;
    const bool owned = path == 2 || path == 5;
    const bool tracked = backend == unimem::Backend::Standard
        ? path >= 3 : index % 4 == 0;

    std::unique_ptr<unimem::Memory> heap;
    if (use_arena) {
        heap.reset(new unimem::Memory(unimem::Memory::heap(backend,
            tracked ? unimem::StatisticsMode::Basic : unimem::StatisticsMode::Disabled)));
    }
    auto& target = use_arena ? *heap : unimem::Memory::global(backend);

    // Every registered case is a repeated allocation/reallocation/free workload.
    for (unsigned cycle = 0; cycle < 64; ++cycle) {
        const auto grown = size + 13 + cycle % 3;
        if (owned) {
            std::vector<unimem::OwnedBlock> blocks;
            blocks.reserve(8);
            for (unsigned slot = 0; slot < 8; ++slot) {
                blocks.emplace_back(target.make_block(size, alignment));
                auto& block = blocks.back();
                check(block.size() == size, "block size");
                if (size != 0) {
                    check(aligned(block.data(), alignment), "block alignment");
                    std::memset(block.data(), 0x5A, size);
                }
                block.resize(grown);
                check(block.size() == grown && aligned(block.data(), alignment),
                      "grown block");
                const auto* data = static_cast<const unsigned char*>(block.data());
                for (std::size_t i = 0; i < size; ++i) {
                    check(data[i] == 0x5A, "block preservation");
                }
            }
        } else {
            std::array<void*, 8> pointers{};
            for (auto& pointer : pointers) {
                pointer = zeroed ? target.allocate_zeroed(size, alignment)
                                 : target.allocate(size, alignment);
                if (size == 0) {
                    check(pointer == nullptr, "empty allocation");
                } else {
                    check(pointer != nullptr && aligned(pointer, alignment),
                          "allocation alignment");
                    if (zeroed) {
                        const auto* data = static_cast<const unsigned char*>(pointer);
                        for (std::size_t i = 0; i < size; ++i) {
                            check(data[i] == 0, "zeroed allocation");
                        }
                    }
                    std::memset(pointer, 0x5A, size);
                }
            }
            for (auto& pointer : pointers) {
                pointer = zeroed
                    ? target.reallocate_zeroed(pointer, size, grown, alignment)
                    : target.reallocate(pointer, size, grown, alignment);
                check(pointer != nullptr && aligned(pointer, alignment),
                      "reallocation alignment");
                const auto* data = static_cast<const unsigned char*>(pointer);
                for (std::size_t i = 0; i < size; ++i) {
                    check(data[i] == 0x5A, "reallocation preservation");
                }
                if (zeroed) {
                    for (std::size_t i = size; i < grown; ++i) {
                        check(data[i] == 0, "zeroed growth");
                    }
                }
            }
            for (auto* pointer : pointers) {
                target.deallocate(pointer, grown, alignment);
            }
        }
        if (tracked) {
            check(target.statistics()->live_bytes == 0,
                  "matrix leaked requested bytes");
        }
    }
}

void stress(unimem::Backend backend) {
    for (bool dedicated : {false, true}) {
        if (dedicated && !unimem::capabilities(backend).heap) { continue; }
        std::unique_ptr<unimem::Memory> heap;
        if (dedicated) {
            heap.reset(new unimem::Memory(unimem::Memory::heap(
                backend, unimem::StatisticsMode::Basic)));
        }
        auto& target = dedicated ? *heap : unimem::Memory::global(backend);
        std::atomic<bool> okay{true};
        std::array<std::thread, 8> workers;
        for (unsigned worker = 0; worker < workers.size(); ++worker) {
            workers[worker] = std::thread([&, worker] {
                try {
                    std::vector<int, unimem::Allocator<int>> values(
                        target.allocator<int>());
                    for (unsigned i = 0; i < 5000; ++i) {
                        const std::size_t size = 1 + (i * 37 + worker * 13) % 4096;
                        const std::size_t alignment = std::size_t{1} <<
                            (3 + (i + worker) % 6);
                        auto* block = target.allocate(size, alignment);
                        check(aligned(block, alignment), "stress alignment");
                        std::memset(block, 0xA5, size);
                        auto* next = target.reallocate(block, size, size + 19,
                                                       alignment);
                        check(static_cast<unsigned char*>(next)[0] == 0xA5,
                              "stress preservation");
                        target.deallocate(next, size + 19, alignment);
                        values.push_back(static_cast<int>(i));
                        if (values.size() > 64) { values.clear(); }
                    }
                } catch (...) {
                    okay.store(false, std::memory_order_relaxed);
                }
            });
        }
        for (auto& worker : workers) { worker.join(); }
        check(okay.load(std::memory_order_relaxed), "stress worker failed");
        const auto stats = target.statistics();
        check(stats && stats->live_bytes == 0 && stats->peak_live_bytes > 0,
              "stress statistics");
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc < 3) { throw std::invalid_argument("mode backend [index]"); }
        const auto backend = parse_backend(argv[2]);
        check(unimem::available(backend), "backend is unavailable");
        const std::string mode = argv[1];
        bool basic = true;
        if (mode == "matrix" && argc == 4) {
            const auto index = std::stoul(argv[3]);
            basic = backend == unimem::Backend::Standard ? index / 40 >= 3 : index % 4 == 0;
        }
        unimem::Memory::configure_global(backend, basic
            ? unimem::StatisticsMode::Basic : unimem::StatisticsMode::Disabled);
        if (mode == "smoke") { smoke(backend); }
        else if (mode == "boundary") { boundaries(backend); }
        else if (mode == "stress") { stress(backend); }
        else if (mode == "matrix" && argc == 4) {
            const auto index = std::stoul(argv[3]);
            check(index < 240, "case index exceeds matrix");
            matrix(backend, static_cast<unsigned>(index));
        } else { throw std::invalid_argument("invalid test mode"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "UniMemory conformance failure: " << error.what() << '\n';
        return 1;
    }
}
