#include "diagnostics/compiled-bench-config.h"
#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <memory_resource>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
std::uint64_t last_checksum = 0;
volatile std::uintptr_t observed_address = 0;
void observe(const void* pointer) { observed_address = reinterpret_cast<std::uintptr_t>(pointer); }

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
                observe(pointer);
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
                if (static_cast<unsigned char*>(slot.pointer)[0] != 7) {
                    throw std::runtime_error("mixed-lifetime content mismatch");
                }
                checksum += static_cast<unsigned char*>(slot.pointer)[0];
                memory.deallocate(slot.pointer, slot.size, alignment);
                slot = {};
            } else {
                slot.size = bytes * (1 + ((state >> 20) % 4));
                slot.pointer = memory.allocate(slot.size, alignment);
                observe(slot.pointer);
                static_cast<unsigned char*>(slot.pointer)[0] = 7;
            }
        }
        for (auto& slot : slots) {
            if (slot.pointer) {
                if (static_cast<unsigned char*>(slot.pointer)[0] != 7) {
                    throw std::runtime_error("mixed-lifetime cleanup content mismatch");
                }
                memory.deallocate(slot.pointer, slot.size, alignment);
            }
        }
        return checksum;
    }
    for (std::size_t i = 0; i < operations; ++i) {
        switch (workload) {
        case Workload::Raw: {
            auto* pointer = memory.allocate(bytes, alignment);
            observe(pointer);
            static_cast<unsigned char*>(pointer)[0] = static_cast<unsigned char>(i);
            checksum += static_cast<unsigned char*>(pointer)[0];
            memory.deallocate(pointer, bytes, alignment);
            break;
        }
        case Workload::Zeroed: {
            auto* pointer = memory.allocate_zeroed(bytes, alignment);
            observe(pointer);
            checksum += static_cast<unsigned char*>(pointer)[0];
            memory.deallocate(pointer, bytes, alignment);
            break;
        }
        case Workload::Reallocate: {
            auto* pointer = memory.allocate(bytes, alignment);
            static_cast<unsigned char*>(pointer)[0] = 7;
            pointer = memory.reallocate(pointer, bytes, bytes * 2, alignment);
            observe(pointer);
            checksum += static_cast<unsigned char*>(pointer)[0];
            memory.deallocate(pointer, bytes * 2, alignment);
            break;
        }
        case Workload::ReallocateZeroed: {
            auto* pointer = memory.allocate(bytes, alignment);
            static_cast<unsigned char*>(pointer)[0] = 7;
            pointer = memory.reallocate_zeroed(pointer, bytes, bytes * 2,
                                               alignment);
            observe(pointer);
            checksum += static_cast<unsigned char*>(pointer)[0];
            checksum += static_cast<unsigned char*>(pointer)[bytes];
            memory.deallocate(pointer, bytes * 2, alignment);
            break;
        }
        case Workload::OwnedBlock: {
            auto block = memory.make_block(bytes, alignment);
            static_cast<unsigned char*>(block.data())[0] = 7;
            block.resize(bytes * 2);
            observe(block.data());
            checksum += static_cast<unsigned char*>(block.data())[0];
            break;
        }
        case Workload::TypedRaw: {
            auto* pointer = memory.allocate_objects<std::byte>(bytes);
            observe(pointer);
            pointer[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(pointer[0]);
            memory.deallocate_objects(pointer, bytes);
            break;
        }
        case Workload::CreateDestroy: {
            auto* object = memory.create<Payload>();
            observe(object);
            object->bytes[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(object->bytes[0]);
            memory.destroy(object);
            break;
        }
        case Workload::Object: {
            auto object = memory.make_unique<Payload>();
            observe(object.get());
            object->bytes[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(object->bytes[0]);
            break;
        }
        case Workload::SharedObject: {
            auto object = memory.make_shared<Payload>();
            observe(object.get());
            object->bytes[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(object->bytes[0]);
            break;
        }
        case Workload::Array: {
            auto array = memory.make_unique_array<std::byte>(bytes);
            observe(array.get());
            array[0] = std::byte{7};
            checksum += std::to_integer<unsigned>(array[0]);
            break;
        }
        case Workload::StdVector: {
            std::vector<std::uint64_t, unimem::Allocator<std::uint64_t>> values(
                memory.allocator<std::uint64_t>());
            for (std::uint64_t j = 0; j < 32; ++j) { values.push_back(j); }
            observe(values.data());
            checksum += values.back();
            break;
        }
        case Workload::PmrVector: {
            std::pmr::vector<std::uint64_t> values(memory.resource());
            for (std::uint64_t j = 0; j < 32; ++j) { values.push_back(j); }
            observe(values.data());
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
    // Repeat the original8192-live-block handoff without increasing live memory
    // or timing thread creation/join when calibrated operation counts grow.
    constexpr std::size_t batch = 8192;
    const auto rounds = (count + batch - 1) / batch;
    std::vector<void*> pointers((std::min)(batch, count));
    std::vector<std::uint64_t> checksums(threads);
    std::vector<std::thread> workers;
    std::barrier phase(static_cast<std::ptrdiff_t>(threads + 1));
    std::barrier ready(static_cast<std::ptrdiff_t>(threads + 1));
    workers.reserve(threads);
    for (std::size_t thread = 0; thread < threads; ++thread) {
        workers.emplace_back([&, thread] {
            ready.arrive_and_wait();
            for (std::size_t round = 0; round < rounds; ++round) {
                const auto current = (std::min)(batch, count - round * batch);
                phase.arrive_and_wait();
                const auto first = thread * current / threads;
                const auto last = (thread + 1) * current / threads;
                for (auto i = first; i < last; ++i) {
                    if (static_cast<unsigned char*>(pointers[i])[0] != 1) { std::terminate(); }
                    checksums[thread] += static_cast<unsigned char*>(pointers[i])[0];
                    memory.deallocate(pointers[i], bytes, alignment);
                }
                phase.arrive_and_wait();
            }
        });
    }
    ready.arrive_and_wait();
    const auto start = Clock::now();
    for (std::size_t round = 0; round < rounds; ++round) {
        const auto current = (std::min)(batch, count - round * batch);
        for (std::size_t i = 0; i < current; ++i) {
            pointers[i] = memory.allocate(bytes, alignment);
            observe(pointers[i]);
            static_cast<unsigned char*>(pointers[i])[0] = 1;
        }
        phase.arrive_and_wait();
        phase.arrive_and_wait();
    }
    const auto end = Clock::now();
    for (auto& worker : workers) { worker.join(); }
    last_checksum = 0;
    for (auto checksum : checksums) { last_checksum += checksum; }
    if (last_checksum != count) { throw std::runtime_error("cross-thread checksum mismatch"); }
    return std::chrono::duration<double, std::nano>(end - start).count()
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
    const auto remainder = operations % 256;
    auto expected = std::uint64_t{7} * operations;
    if (workload == Workload::Raw) {
        expected = (operations / 256) * 32640 + remainder * (remainder - (remainder != 0)) / 2;
    } else if (workload == Workload::Zeroed) { expected = 0; }
    else if (workload == Workload::Batch) { expected = ((operations + 63) / 64) * 64 * 7; }
    else if (workload == Workload::StdVector || workload == Workload::PmrVector) { expected = 31 * operations; }
    if (workload != Workload::MixedLifetime && workload != Workload::DetailedStatistics && checksum != expected) {
        throw std::runtime_error("sweep workload checksum mismatch");
    }
    static volatile std::uint64_t consumed = 0;
    consumed = consumed + checksum;
    last_checksum = checksum;
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
    compiled_benchmark_backend(backend.value);
    std::unique_ptr<unimem::Memory> heap;
    if (scenario.heap) {
        heap.reset(new unimem::Memory(unimem::Memory::heap(
            scenario.statistics)));
    }
    auto& target = scenario.heap ? *heap : unimem::Memory::global();
    measure(target, scenario.workload, scenario.bytes,
            scenario.alignment, (std::min)(operations / 4 + 1, std::size_t{1024}), scenario.threads);
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
                              heap ? unimem::StatisticsMode::Disabled
                                   : unimem::detail::build_statistics, heap});
        }
    }
    for (const auto heap : {false, true}) {
        result.push_back({Workload::DetailedStatistics, 0, 0, 1,
                          heap ? unimem::StatisticsMode::Disabled
                               : unimem::detail::build_statistics, heap});
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
        observe(pointer);
        pointer[0] = static_cast<unsigned char>(i);
        checksum += pointer[0];
        heap.rewind(mark);
    }
    const auto end = Clock::now();
    static volatile std::uint64_t consumed = 0;
    consumed = consumed + checksum;
    last_checksum = checksum;
    return std::chrono::duration<double, std::nano>(end - start).count()
        / static_cast<double>(operations);
}

std::string scenario_key(const Scenario& scenario) {
    return std::string(name(scenario.workload)) + ":" + std::to_string(scenario.bytes)
        + ":" + std::to_string(scenario.alignment) + ":" + std::to_string(scenario.threads)
        + ":" + (scenario.heap ? "yes" : "no");
}

struct Calibration {
    std::size_t count = 0, limit = 0;
    std::vector<std::pair<std::size_t, double>> pilots;
    bool sustained = false, capped = false;
    double estimated_seconds = 0;
};

template<class Measure>
Calibration calibrate(Measure&& measure, std::size_t count, std::size_t maximum, const std::string& key) {
    // Calibrate complete operations for this SDK/statistics profile. Python
    // retains these per-profile counts for every reported process trial.
    Calibration result;
    result.limit = maximum;
    double fastest = 0, slowest = 0;
    for (;;) {
        fastest = 1e100;
        slowest = 0;
        for (unsigned repeat = 0; repeat < 3; ++repeat) {
            std::cerr << "CALIBRATION_START," << key << ',' << count << ',' << repeat + 1 << '\n';
            const auto seconds = measure(count) * count / 1e9;
            if (!(seconds > 0)) { throw std::runtime_error("nonpositive full calibration duration"); }
            result.pilots.emplace_back(count, seconds);
            std::cerr << "CALIBRATION_PILOT," << key << ',' << count << ',' << repeat + 1 << ',' << seconds << '\n';
            fastest = (std::min)(fastest, seconds);
            slowest = (std::max)(slowest, seconds);
        }
        if (fastest >= .125 || count == maximum || slowest >= 2) { break; }
        count = (std::min)(count * 2, maximum);
    }
    const auto desired = (std::max)(count, static_cast<std::size_t>(count * .35 / fastest) + 1);
    const auto budget = (std::max)(std::size_t{64}, static_cast<std::size_t>(count * 2 / slowest) / 64 * 64);
    result.count = (std::min)((desired + 63) / 64 * 64, (std::min)(maximum, budget));
    result.sustained = fastest >= .125;
    result.capped = result.count < desired;
    result.estimated_seconds = slowest * result.count / count;
    return result;
}

void emit_calibration(const char* backend, const Scenario& scenario, const Calibration& sample, bool oversubscribed) {
    std::cout << backend << ',' << name(scenario.workload) << ',' << scenario.bytes << ',' << scenario.alignment << ','
              << scenario.threads << ',' << (scenario.statistics == unimem::StatisticsMode::Basic ? "basic" : "disabled")
              << ',' << (scenario.heap ? "yes" : "no") << ',' << sample.count << ',';
    for (std::size_t i = 0; i < sample.pilots.size(); ++i) {
        if (i) { std::cout << ';'; }
        std::cout << sample.pilots[i].first;
    }
    std::cout << ',';
    for (std::size_t i = 0; i < sample.pilots.size(); ++i) {
        if (i) { std::cout << ';'; }
        std::cout << sample.pilots[i].second;
    }
    std::cout << ',' << (sample.sustained ? "true" : "false") << ',' << (sample.capped ? "true" : "false")
              << ',' << sample.limit << ',' << (oversubscribed ? "true" : "false") << ',' << sample.estimated_seconds << '\n';
    std::cout.flush();
}

void emit(const char* backend, const Scenario& scenario, std::size_t count, double time, bool calibration = false) {
    std::cout << backend << ',' << name(scenario.workload)
              << ',' << scenario.bytes << ',' << scenario.alignment
              << ',' << scenario.threads << ','
              << (scenario.statistics == unimem::StatisticsMode::Basic ? "basic" : "disabled")
              << ',' << (scenario.heap ? "yes" : "no");
    if (calibration) { std::cout << ',' << count << '\n'; return; }
    std::cout << ",1," << count << ',' << time << ',' << time << ',' << time << ','
              << time << ',' << time << ',' << time * count / 1e9 << ',' << last_checksum << '\n';
    std::cout.flush();
}

}

int main(int argc, char** argv) {
    try {
        bool full = false;
        bool calibration = false;
        unsigned seed = 0xC0FFEE;
        std::size_t effective_cpus = (std::max)(1U, std::thread::hardware_concurrency());
        std::map<std::string, std::size_t> counts;
        std::cout << std::setprecision(17);
        constexpr bool basic = unimem::detail::build_statistics == unimem::StatisticsMode::Basic;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--full") { full = true; }
            else if (argument == "--calibrate") { calibration = true; }
            else if (argument == "--seed" && i + 1 < argc) { seed = static_cast<unsigned>(std::stoul(argv[++i])); }
            else if (argument == "--effective-cpus" && i + 1 < argc) {
                effective_cpus = std::stoull(argv[++i]);
                if (!effective_cpus) { throw std::invalid_argument("nonzero effective CPU capacity required"); }
            }
            else if (argument == "--operations-file" && i + 1 < argc) {
                std::ifstream input(argv[++i]);
                if (!input) { throw std::invalid_argument("cannot read shared operations file"); }
                std::string line;
                while (std::getline(input, line)) {
                    const auto separator = line.find(',');
                    if (separator == std::string::npos) { throw std::invalid_argument("invalid operations file"); }
                    const auto count = std::stoull(line.substr(separator + 1));
                    if (!count || !counts.emplace(line.substr(0, separator), count).second) {
                        throw std::invalid_argument("zero or duplicate shared operations count");
                    }
                }
            }
            else if (argument == "--basic") {
                if (!basic) { throw std::invalid_argument("--basic requires UNIMEMORY_STATISTICS=ON"); }
            }
            else { throw std::invalid_argument("unknown benchmark argument"); }
        }
        for (const auto backend : backends) {
            if (backend.value == unimem::detail::build_backend) {
                compiled_benchmark_configuration(backend.value, basic
                    ? unimem::StatisticsMode::Basic : unimem::StatisticsMode::Disabled);
            }
        }
        const auto operations = full ? 8192U : 2048U;
        std::cout << "backend,workload,bytes,alignment,threads,statistics,heap,";
        if (calibration) {
            std::cout << "operations,pilot_operations,pilot_seconds,sustained_goal_met,capped,count_limit,oversubscribed,estimated_seconds\n";
        }
        else {
            std::cout << "repetitions,operations,median_ns_per_operation,min_ns_per_operation,mean_ns_per_operation,"
                         "max_ns_per_operation,samples_ns_per_operation,seconds,checksum\n";
        }
        std::mt19937 generator(seed);
        auto cases = scenarios(full);
        std::shuffle(cases.begin(), cases.end(), generator);
        for (const auto& scenario : cases) {
            if (!scenario.heap && ((scenario.statistics == unimem::StatisticsMode::Basic) != basic)) {
                continue;
            }
            if (scenario.heap && !basic && scenario.statistics == unimem::StatisticsMode::Basic) {
                continue;
            }
            std::vector<BackendName> enabled;
            for (const auto backend : backends) {
                if (backend.value == unimem::detail::build_backend &&
                    (!scenario.heap || unimem::capabilities().heap) &&
                    (scenario.workload != Workload::DetailedStatistics ||
                     unimem::capabilities().detailed_statistics)) {
                    enabled.push_back(backend);
                }
            }
            for (const auto& backend : enabled) {
                auto count = std::size_t{operations};
                if (calibration) {
                    const auto oversubscribed = scenario.threads > effective_cpus;
                    const auto maximum = scenario.workload == Workload::CrossThread && oversubscribed
                        ? std::size_t{8192} * 32 : std::size_t{100000000};
                    const auto sample = calibrate([&](auto current) { return run(backend, scenario, current); }, 256, maximum,
                                                  std::string(backend.name) + ':' + scenario_key(scenario) + ':' +
                                                  (scenario.statistics == unimem::StatisticsMode::Basic ? "basic" : "disabled"));
                    emit_calibration(backend.name, scenario, sample, oversubscribed);
                    continue;
                } else if (!counts.empty()) {
                    count = counts.at(scenario_key(scenario));
                }
                const auto time = calibration ? 0.0 : run(backend, scenario, count);
                emit(backend.name, scenario, count, time, calibration);
            }
        }
        for (const auto bytes : full ? std::vector<std::size_t>{64, 256, 4096, 65536}
                                     : std::vector<std::size_t>{64, 4096}) {
            for (const auto alignment : full ? std::vector<std::size_t>{16, 64, 256}
                                             : std::vector<std::size_t>{16}) {
                const auto key = "stack_mark_rewind:" + std::to_string(bytes) + ":" + std::to_string(alignment) + ":1:no";
                auto count = std::size_t{operations};
                if (calibration) {
                    const auto sample = calibrate([&](auto current) { return stack_mark_rewind(bytes, alignment, current); }, 256,
                                                  std::size_t{100000000}, "fixed_buffer:" + key);
                    // Reuse the same calibration CSV layout for the fixed-buffer scenario.
                    std::cout << "fixed_buffer,stack_mark_rewind," << bytes << ',' << alignment << ",1,disabled,no," << sample.count << ',';
                    for (std::size_t i = 0; i < sample.pilots.size(); ++i) { if (i) { std::cout << ';'; } std::cout << sample.pilots[i].first; }
                    std::cout << ',';
                    for (std::size_t i = 0; i < sample.pilots.size(); ++i) { if (i) { std::cout << ';'; } std::cout << sample.pilots[i].second; }
                    std::cout << ',' << (sample.sustained ? "true" : "false") << ',' << (sample.capped ? "true" : "false")
                              << ',' << sample.limit << ",false," << sample.estimated_seconds << '\n';
                    std::cout.flush();
                    continue;
                } else if (!counts.empty()) { count = counts.at(key); }
                stack_mark_rewind(bytes, alignment, (std::min)(count, std::size_t{1024}));
                const auto time = stack_mark_rewind(bytes, alignment, calibration ? 256 : count);
                std::cout << "fixed_buffer,stack_mark_rewind," << bytes << ','
                          << alignment << ",1,disabled,no,1," << count << ',' << time << ',' << time << ','
                          << time << ',' << time << ',' << time << ',' << time * count / 1e9 << ','
                          << last_checksum << '\n';
                std::cout.flush();
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "UniMemory benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
