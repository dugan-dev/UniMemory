#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct BackendName { unimem::Backend value; const char* name; };
constexpr std::array<BackendName, 3> backends{{
    {unimem::Backend::Standard, "standard"},
    {unimem::Backend::Mimalloc, "mimalloc"},
    {unimem::Backend::Jemalloc, "jemalloc"}}};

enum class Workload {
    Raw, Zeroed, Reallocate, ReallocateZeroed, Batch, MixedLifetime,
    OwnedBlock, TypedRaw, CreateDestroy, Object, SharedObject, Array,
    StdVector, PmrVector, CrossThread, DetailedStatistics
};

const char* name(Workload workload) {
    switch (workload) {
    case Workload::Raw: return "raw";
    case Workload::Zeroed: return "zeroed";
    case Workload::Reallocate: return "reallocate";
    case Workload::ReallocateZeroed: return "reallocate_zeroed";
    case Workload::Batch: return "batch";
    case Workload::MixedLifetime: return "mixed_lifetime";
    case Workload::OwnedBlock: return "owned_block";
    case Workload::TypedRaw: return "typed_raw";
    case Workload::CreateDestroy: return "create_destroy";
    case Workload::Object: return "object";
    case Workload::SharedObject: return "shared_object";
    case Workload::Array: return "array";
    case Workload::StdVector: return "std_vector";
    case Workload::PmrVector: return "pmr_vector";
    case Workload::CrossThread: return "cross_thread";
    case Workload::DetailedStatistics: return "detailed_statistics";
    }
    return "unknown";
}

struct Payload { std::array<std::byte, 64> bytes{}; };

std::uint64_t serial(unimem::Memory& memory, Workload workload,
                     std::size_t bytes, std::size_t alignment,
                     std::size_t operations) {
    std::uint64_t checksum = 0;
    if (workload == Workload::Batch) {
        std::array<void*, 64> pointers{};
        for (std::size_t round = 0; round < (operations + 63) / 64; ++round) {
            for (auto& pointer : pointers) {
                pointer = memory.allocate(bytes, alignment);
                static_cast<unsigned char*>(pointer)[0] = 7;
            }
            for (auto* pointer : pointers) {
                checksum += static_cast<unsigned char*>(pointer)[0];
                memory.deallocate(pointer, bytes, alignment);
            }
        }
        return checksum;
    }
    if (workload == Workload::MixedLifetime) {
        struct Slot { void* pointer = nullptr; std::size_t size = 0; };
        std::array<Slot, 256> slots{};
        std::uint32_t state = 0x12345678u;
        for (std::size_t i = 0; i < operations; ++i) {
            state = state * 1664525u + 1013904223u;
            auto& slot = slots[(state >> 8) % slots.size()];
            if (slot.pointer) {
                checksum += static_cast<unsigned char*>(slot.pointer)[0];
                memory.deallocate(slot.pointer, slot.size, alignment);
                slot = {};
            } else {
                slot.size = bytes * (1 + ((state >> 20) % 4));
                slot.pointer = memory.allocate(slot.size, alignment);
                static_cast<unsigned char*>(slot.pointer)[0] = 7;
            }
        }
        for (auto& slot : slots) {
            if (slot.pointer) { memory.deallocate(slot.pointer, slot.size, alignment); }
        }
        return checksum;
    }
    for (std::size_t i = 0; i < operations; ++i) {
        switch (workload) {
        case Workload::Raw: {
            auto* pointer = memory.allocate(bytes, alignment);
            static_cast<unsigned char*>(pointer)[0] = static_cast<unsigned char>(i);
            checksum += static_cast<unsigned char*>(pointer)[0];
            memory.deallocate(pointer, bytes, alignment);
            break;
        }
        case Workload::Zeroed: {
            auto* pointer = memory.allocate_zeroed(bytes, alignment);
            checksum += static_cast<unsigned char*>(pointer)[0];
            memory.deallocate(pointer, bytes, alignment);
            break;
        }
        case Workload::Reallocate: {
            auto* pointer = memory.allocate(bytes, alignment);
            static_cast<unsigned char*>(pointer)[0] = 7;
            pointer = memory.reallocate(pointer, bytes, bytes * 2, alignment);
            checksum += static_cast<unsigned char*>(pointer)[0];
            memory.deallocate(pointer, bytes * 2, alignment);
            break;
        }
        case Workload::ReallocateZeroed: {
            auto* pointer = memory.allocate(bytes, alignment);
            static_cast<unsigned char*>(pointer)[0] = 7;
            pointer = memory.reallocate_zeroed(pointer, bytes, bytes * 2,
                                               alignment);
            checksum += static_cast<unsigned char*>(pointer)[0];
            checksum += static_cast<unsigned char*>(pointer)[bytes];
            memory.deallocate(pointer, bytes * 2, alignment);
            break;
        }
        case Workload::OwnedBlock: {
            auto block = memory.make_block(bytes, alignment);
            static_cast<unsigned char*>(block.data())[0] = 7;
            block.resize(bytes * 2);
            checksum += static_cast<unsigned char*>(block.data())[0];
            break;
        }
        case Workload::TypedRaw: {
            auto* pointer = memory.allocate_objects<std::byte>(bytes);
            pointer[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(pointer[0]);
            memory.deallocate_objects(pointer, bytes);
            break;
        }
        case Workload::CreateDestroy: {
            auto* object = memory.create<Payload>();
            object->bytes[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(object->bytes[0]);
            memory.destroy(object);
            break;
        }
        case Workload::Object: {
            auto object = memory.make_unique<Payload>();
            object->bytes[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(object->bytes[0]);
            break;
        }
        case Workload::SharedObject: {
            auto object = memory.make_shared<Payload>();
            object->bytes[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(object->bytes[0]);
            break;
        }
        case Workload::Array: {
            auto array = memory.make_unique_array<std::byte>(bytes);
            array[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(array[0]);
            break;
        }
        case Workload::StdVector: {
            std::vector<std::uint64_t, unimem::Allocator<std::uint64_t>> values(
                memory.allocator<std::uint64_t>());
            for (std::uint64_t j = 0; j < 32; ++j) { values.push_back(j); }
            checksum += values.back();
            break;
        }
        case Workload::PmrVector: {
            std::pmr::vector<std::uint64_t> values(memory.resource());
            for (std::uint64_t j = 0; j < 32; ++j) { values.push_back(j); }
            checksum += values.back();
            break;
        }
        case Workload::DetailedStatistics: {
            const auto stats = memory.backend_statistics();
            if (stats && stats->allocated_bytes) { checksum += *stats->allocated_bytes; }
            break;
        }
        case Workload::CrossThread: break;
        case Workload::Batch: break;
        case Workload::MixedLifetime: break;
        }
    }
    return checksum;
}

double cross_thread(unimem::Memory& memory, std::size_t bytes,
                    std::size_t alignment, std::size_t count,
                    std::size_t threads) {
    std::vector<void*> pointers(count);
    std::atomic<std::size_t> ready{0};
    std::atomic<bool> release{false};
    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (std::size_t thread = 0; thread < threads; ++thread) {
        workers.emplace_back([&, thread] {
            ready.fetch_add(1, std::memory_order_release);
            while (!release.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            const auto first = thread * count / threads;
            const auto last = (thread + 1) * count / threads;
            for (auto i = first; i < last; ++i) {
                memory.deallocate(pointers[i], bytes, alignment);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) != threads) {
        std::this_thread::yield();
    }
    const auto start = Clock::now();
    for (auto& pointer : pointers) {
        pointer = memory.allocate(bytes, alignment);
        static_cast<unsigned char*>(pointer)[0] = 1;
    }
    release.store(true, std::memory_order_release);
    for (auto& worker : workers) { worker.join(); }
    return std::chrono::duration<double, std::nano>(Clock::now() - start).count()
        / static_cast<double>(count);
}

double measure(unimem::Memory& memory, Workload workload,
               std::size_t bytes, std::size_t alignment,
               std::size_t operations, std::size_t threads) {
    if (workload == Workload::CrossThread) {
        return cross_thread(memory, bytes, alignment, operations, threads);
    }
    const auto start = Clock::now();
    const auto checksum = serial(memory, workload, bytes, alignment, operations);
    const auto end = Clock::now();
    static volatile std::uint64_t consumed = 0;
    consumed = consumed + checksum;
    return std::chrono::duration<double, std::nano>(end - start).count()
        / static_cast<double>(operations);
}

struct Scenario {
    Workload workload;
    std::size_t bytes;
    std::size_t alignment;
    std::size_t threads;
    unimem::StatisticsMode statistics;
    bool heap;
};

double run(const BackendName& backend, const Scenario& scenario,
           std::size_t operations) {
    std::unique_ptr<unimem::Memory> heap;
    if (scenario.heap) {
        heap.reset(new unimem::Memory(unimem::Memory::heap(
            backend.value, scenario.statistics)));
    }
    auto& target = scenario.heap ? *heap : unimem::Memory::global(backend.value);
    measure(target, scenario.workload, scenario.bytes,
            scenario.alignment, operations / 4 + 1, scenario.threads);
    const auto result = measure(target, scenario.workload, scenario.bytes,
                                scenario.alignment, operations, scenario.threads);
    if (auto stats = target.statistics()) {
        if (stats->live_bytes != 0) { throw std::runtime_error("benchmark leaked bytes"); }
    }
    return result;
}

std::vector<Scenario> scenarios(bool full) {
    std::vector<Scenario> result;
    for (const auto bytes : full ? std::vector<std::size_t>{64, 256, 4096, 65536}
                                 : std::vector<std::size_t>{64, 4096}) {
        for (const auto alignment : full ? std::vector<std::size_t>{16, 64, 256}
                                         : std::vector<std::size_t>{16}) {
            for (const auto statistics : {unimem::StatisticsMode::Disabled,
                                          unimem::StatisticsMode::Basic}) {
                for (const auto heap : {false, true}) {
                    for (const auto workload : {
                             Workload::Raw, Workload::Zeroed,
                             Workload::Reallocate, Workload::ReallocateZeroed,
                             Workload::Batch,
                             Workload::MixedLifetime, Workload::OwnedBlock,
                             Workload::TypedRaw, Workload::CreateDestroy,
                             Workload::Object, Workload::SharedObject,
                             Workload::Array,
                             Workload::StdVector, Workload::PmrVector}) {
                        auto payload_bytes = bytes;
                        auto payload_alignment = alignment;
                        if (workload == Workload::TypedRaw || workload == Workload::Array) {
                            if (alignment != 16) { continue; }
                            payload_alignment = alignof(std::byte);
                        } else if (workload == Workload::CreateDestroy ||
                                   workload == Workload::Object ||
                                   workload == Workload::SharedObject) {
                            if (bytes != 64 || alignment != 16) { continue; }
                            payload_bytes = sizeof(Payload);
                            payload_alignment = alignof(Payload);
                        } else if (workload == Workload::StdVector ||
                                   workload == Workload::PmrVector) {
                            if (bytes != 64 || alignment != 16) { continue; }
                            payload_bytes = 32 * sizeof(std::uint64_t);
                            payload_alignment = alignof(std::uint64_t);
                        }
                        result.push_back({workload, payload_bytes, payload_alignment, 1,
                                          statistics, heap});
                    }
                }
            }
        }
    }
    for (const auto threads : {2U, 4U, 8U}) {
        for (const auto heap : {false, true}) {
            result.push_back({Workload::CrossThread, 64, 16, threads,
                              unimem::StatisticsMode::Disabled, heap});
        }
    }
    for (const auto heap : {false, true}) {
        result.push_back({Workload::DetailedStatistics, 0, 0, 1,
                          unimem::StatisticsMode::Disabled, heap});
    }
    return result;
}

double stack_mark_rewind(std::size_t bytes, std::size_t alignment,
                         std::size_t operations) {
    std::vector<std::byte> backing(bytes + alignment);
    unimem::Memory heap = unimem::Memory::stack(backing.data(), backing.size());
    std::uint64_t checksum = 0;
    const auto start = Clock::now();
    for (std::size_t i = 0; i < operations; ++i) {
        const auto mark = heap.mark();
        auto* pointer = static_cast<unsigned char*>(heap.allocate(bytes, alignment));
        pointer[0] = static_cast<unsigned char>(i);
        checksum += pointer[0];
        heap.rewind(mark);
    }
    const auto end = Clock::now();
    static volatile std::uint64_t consumed = 0;
    consumed = consumed + checksum;
    return std::chrono::duration<double, std::nano>(end - start).count()
        / static_cast<double>(operations);
}

}

int main(int argc, char** argv) {
    try {
        bool full = false, basic = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--full") { full = true; }
            else if (argument == "--basic") { basic = true; }
            else { throw std::invalid_argument("expected --full or --basic"); }
        }
        for (const auto backend : backends) {
            if (unimem::available(backend.value)) {
                unimem::Memory::configure_global(backend.value, basic
                    ? unimem::StatisticsMode::Basic : unimem::StatisticsMode::Disabled);
            }
        }
        const auto reps = full ? 7U : 5U;
        const auto operations = full ? 8192U : 2048U;
        std::cout << "backend,workload,bytes,alignment,threads,statistics,heap,"
                     "repetitions,operations,median_ns_per_operation\n";
        std::mt19937 generator(0xC0FFEE);
        for (const auto& scenario : scenarios(full)) {
            if (basic && scenario.heap) { continue; }
            if (!scenario.heap && ((scenario.statistics == unimem::StatisticsMode::Basic) != basic)) {
                continue;
            }
            std::vector<BackendName> enabled;
            for (const auto backend : backends) {
                if (unimem::available(backend.value) &&
                    (!scenario.heap || unimem::capabilities(backend.value).heap) &&
                    (scenario.workload != Workload::DetailedStatistics ||
                     unimem::capabilities(backend.value).detailed_statistics)) {
                    enabled.push_back(backend);
                }
            }
            std::vector<std::vector<double>> samples(enabled.size());
            const auto count = scenario.workload == Workload::DetailedStatistics
                ? operations / 16 : operations;
            for (unsigned repetition = 0; repetition < reps; ++repetition) {
                std::vector<std::size_t> order;
                for (std::size_t i = 0; i < enabled.size(); ++i) { order.push_back(i); }
                std::shuffle(order.begin(), order.end(), generator);
                for (const auto index : order) {
                    samples[index].push_back(run(enabled[index], scenario, count));
                }
            }
            for (std::size_t i = 0; i < enabled.size(); ++i) {
                auto& times = samples[i];
                std::sort(times.begin(), times.end());
                std::cout << enabled[i].name << ',' << name(scenario.workload)
                          << ',' << scenario.bytes << ',' << scenario.alignment
                          << ',' << scenario.threads << ','
                          << (scenario.statistics == unimem::StatisticsMode::Basic
                              ? "basic" : "disabled")
                          << ',' << (scenario.heap ? "yes" : "no")
                          << ',' << reps << ',' << count << ','
                          << times[times.size() / 2] << '\n';
            }
        }
        if (!basic) for (const auto bytes : full ? std::vector<std::size_t>{64, 256, 4096, 65536}
                                     : std::vector<std::size_t>{64, 4096}) {
            for (const auto alignment : full ? std::vector<std::size_t>{16, 64, 256}
                                             : std::vector<std::size_t>{16}) {
                stack_mark_rewind(bytes, alignment, operations / 4 + 1);
                std::vector<double> samples;
                for (unsigned repetition = 0; repetition < reps; ++repetition) {
                    samples.push_back(stack_mark_rewind(bytes, alignment, operations));
                }
                std::sort(samples.begin(), samples.end());
                std::cout << "fixed_buffer,stack_mark_rewind," << bytes << ','
                          << alignment << ",1,disabled,no," << reps << ','
                          << operations << ',' << samples[samples.size() / 2]
                          << '\n';
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "UniMemory benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
