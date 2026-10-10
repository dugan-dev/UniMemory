#include <unimem/memory.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <barrier>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach/mach.h>
#include <sys/resource.h>
#else
#include <dlfcn.h>
#include <sys/resource.h>
#include <unistd.h>
#endif
#ifdef UNIMEMORY_BENCH_MIMALLOC
#include <mimalloc.h>
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>
#endif

namespace {
using namespace unimem;
using Clock = std::chrono::steady_clock;
volatile std::uintptr_t observed = 0;
struct NativeStandard {
    void* allocate(std::size_t n) { return ::operator new(n, std::align_val_t(16)); }
    void deallocate(void* p, std::size_t) { ::operator delete(p, std::align_val_t(16)); }
};
#ifdef UNIMEMORY_BENCH_MIMALLOC
struct NativeMimalloc {
    void* allocate(std::size_t n) { return mi_malloc_aligned(n, 16); }
    void deallocate(void* p, std::size_t) { mi_free(p); }
};
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
struct NativeJemalloc {
    void* allocate(std::size_t n) { return je_mallocx(n, MALLOCX_ALIGN(16)); }
    void deallocate(void* p, std::size_t) { je_dallocx(p, 0); }
};
#endif
struct Unified {
    Memory& memory;
    static Memory& get(Backend backend, StatisticsMode mode) {
        Memory::configure_global(backend, mode);
        return Memory::global(backend);
    }
    Unified(Backend backend, StatisticsMode mode) : memory(get(backend, mode)) {}
    void* allocate(std::size_t n) { return memory.allocate(n, 16); }
    void deallocate(void* p, std::size_t n) { memory.deallocate(p, n, 16); }
};

std::size_t usable_bytes(Backend backend, void* pointer) {
#ifdef UNIMEMORY_BENCH_MIMALLOC
    if (backend == Backend::Mimalloc) { return mi_usable_size(pointer); }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    if (backend == Backend::Jemalloc) { return je_sallocx(pointer, 0); }
#endif
    (void)backend;
    (void)pointer;
    return 0; // C++ does not provide a portable usable-size query for new.
}

void environment() {
    std::cout << "{\"memory_bytes\":" << sizeof(Memory)
              << ",\"owned_block_bytes\":" << sizeof(OwnedBlock)
              << ",\"allocator_bytes\":" << sizeof(Allocator<int>);
#ifndef _WIN32
    Dl_info provider{};
    const auto allocate = static_cast<void* (*)(std::size_t)>(&::operator new);
    if (!dladdr(reinterpret_cast<void*>(allocate), &provider) || !provider.dli_fname) {
        throw std::runtime_error("global new provider query failed");
    }
    const std::string library = provider.dli_fname;
    std::cout << ",\"standard_new_provider\":\""
              << library.substr(library.find_last_of('/') + 1) << '\"';
#endif
#ifdef _WIN32
    std::cout << ",\"resident_measurement\":\"GetProcessMemoryInfo working set\"";
#elif defined(__APPLE__)
    std::cout << ",\"resident_measurement\":\"Mach task resident_size\"";
#else
    std::cout << ",\"resident_measurement\":\""
              << (std::ifstream("/proc/self/smaps_rollup")
                  ? "smaps_rollup Rss" : "statm resident (approximate fallback)")
              << '\"';
#endif
#ifdef UNIMEMORY_BENCH_MIMALLOC
    std::cout << ",\"mimalloc_version\":" << mi_version()
              << ",\"mimalloc_redirected\":" << (mi_is_redirected() ? "true" : "false");
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    std::cout << ",\"jemalloc_header_version\":\"" << JEMALLOC_VERSION << '\"';
#endif
    std::cout << "}\n";
}

std::uint64_t rss() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
        throw std::runtime_error("working set query failed");
    }
    return info.WorkingSetSize;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
        reinterpret_cast<task_info_t>(&info), &size) != KERN_SUCCESS) {
        throw std::runtime_error("resident memory query failed");
    }
    return info.resident_size;
#else
    // Page-table totals avoid the asynchronous RSS counters used by statm.
    std::ifstream rollup("/proc/self/smaps_rollup");
    if (rollup) {
        std::string line;
        while (std::getline(rollup, line)) {
            if (line.starts_with("Rss:")) {
                return std::stoull(line.substr(4)) * 1024;
            }
        }
        throw std::runtime_error("smaps_rollup RSS query failed");
    }
    std::uint64_t total = 0, resident = 0;
    std::ifstream input("/proc/self/statm");
    if (!(input >> total >> resident)) { throw std::runtime_error("statm query failed"); }
    return resident * static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
#endif
}
std::uint64_t peak_rss() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
        throw std::runtime_error("peak working set query failed");
    }
    return info.PeakWorkingSetSize;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) { throw std::runtime_error("rusage query failed"); }
#ifdef __APPLE__
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
#endif
}

template<class A> void tails(A& allocator, const std::string& backend,
                            const std::string& path, std::size_t bytes) {
    constexpr std::size_t count = 8192;
    std::vector<double> allocation(count), release(count), overhead(count);
    for (unsigned warmup = 0; warmup < 1024; ++warmup) {
        auto* pointer = allocator.allocate(bytes);
        if (!pointer) { throw std::bad_alloc(); }
        allocator.deallocate(pointer, bytes);
    }
    for (std::size_t index = 0; index < count; ++index) {
        const auto empty_start = Clock::now();
        const auto empty_end = Clock::now();
        overhead[index] = std::chrono::duration<double, std::nano>(empty_end - empty_start).count();
        const auto start = Clock::now();
        auto* pointer = allocator.allocate(bytes);
        const auto allocated = Clock::now();
        if (!pointer) { throw std::bad_alloc(); }
        static_cast<volatile unsigned char*>(pointer)[0] = static_cast<unsigned char>(index);
        observed = reinterpret_cast<std::uintptr_t>(pointer);
        const auto free_start = Clock::now();
        allocator.deallocate(pointer, bytes);
        const auto freed = Clock::now();
        allocation[index] = std::chrono::duration<double, std::nano>(allocated - start).count();
        release[index] = std::chrono::duration<double, std::nano>(freed - free_start).count();
    }
    // Raw operation samples are retained separately by the remote runner.
    std::cout << "backend,path,bytes,sample,allocate_ns,free_ns,clock_ns\n";
    for (std::size_t index = 0; index < count; ++index) {
        std::cout << backend << ',' << path << ',' << bytes << ',' << index << ','
                  << allocation[index] << ',' << release[index] << ',' << overhead[index] << '\n';
    }
}

template<class A> void scaling(A& allocator, const std::string& backend,
                              const std::string& path, std::size_t bytes,
                              unsigned threads, bool handoff) {
    if (threads == 0 || threads > 16) { throw std::invalid_argument("threads must be 1..16"); }
    // Keep 4096 pairs/thread while amortizing scheduler/barrier overhead.
    // Maximum configured live payload: 16 * 512 * 65536 = 512 MiB.
    constexpr unsigned batch = 512, rounds = 8;
    // Initialize every Native/API path on the main thread before worker startup.
    auto* initial = allocator.allocate(bytes);
    if (!initial) { throw std::bad_alloc(); }
    std::memset(initial, 0, bytes);
    allocator.deallocate(initial, bytes);
    std::vector<std::array<void*, batch>> pointers(threads);
    std::vector<std::thread> workers;
    std::barrier phase(static_cast<std::ptrdiff_t>(threads));
    std::barrier start(static_cast<std::ptrdiff_t>(threads + 1));
    std::barrier ready(static_cast<std::ptrdiff_t>(threads + 1));
    std::atomic<bool> failed{false};
    for (unsigned id = 0; id < threads; ++id) {
        workers.emplace_back([&, id] {
            try {
                for (unsigned warmup = 0; warmup < 64; ++warmup) {
                    auto* pointer = allocator.allocate(bytes);
                    if (!pointer) { throw std::bad_alloc(); }
                    static_cast<volatile unsigned char*>(pointer)[0] = 0;
                    allocator.deallocate(pointer, bytes);
                }
            } catch (...) { failed.store(true); }
            ready.arrive_and_wait();
            start.arrive_and_wait();
            for (unsigned round = 0; round < rounds; ++round) {
                for (auto& pointer : pointers[id]) {
                    try {
                        pointer = allocator.allocate(bytes);
                        if (!pointer) { throw std::bad_alloc(); }
                        std::memset(pointer, static_cast<int>(id + 1), bytes);
                    } catch (...) { pointer = nullptr; failed.store(true); }
                }
                phase.arrive_and_wait();
                const auto owner = handoff ? (id + 1) % threads : id;
                for (auto pointer : pointers[owner]) {
                    if (!pointer) { continue; }
                    const auto* data = static_cast<const unsigned char*>(pointer);
                    if (data[0] != owner + 1 || data[bytes - 1] != owner + 1) { failed.store(true); }
                    allocator.deallocate(pointer, bytes);
                }
                phase.arrive_and_wait();
            }
        });
    }
    ready.arrive_and_wait();
    const auto before_rss = rss();
    const auto begin = Clock::now();
    start.arrive_and_wait();
    for (auto& worker : workers) { worker.join(); }
    const auto seconds = std::chrono::duration<double>(Clock::now() - begin).count();
    if (failed.load()) { throw std::runtime_error("scaling allocation or content failure"); }
    std::cout << "backend,path,workload,bytes,threads,operations,seconds,operations_per_second,baseline_rss,final_rss,peak_rss\n"
              << backend << ',' << path << ',' << (handoff ? "handoff" : "same_thread") << ','
              << bytes << ',' << threads << ',' << threads * batch * rounds << ',' << seconds << ','
              << (threads * batch * rounds / seconds) << ',' << before_rss << ',' << rss() << ',' << peak_rss() << '\n';
}

template<class A> void latency(A& allocator, const std::string& backend,
                               const std::string& path, std::size_t bytes) {
    constexpr unsigned repetitions = 9;
    constexpr std::size_t operations = 200000;
    std::vector<double> samples;
    for (unsigned repeat = 0; repeat <= repetitions; ++repeat) {
        const auto start = Clock::now();
        for (std::size_t i = 0; i < operations; ++i) {
            void* p = allocator.allocate(bytes);
            if (!p) { throw std::bad_alloc(); }
            // Keep the byte touch observable on native and unified paths alike.
            static_cast<volatile unsigned char*>(p)[0] = static_cast<unsigned char>(i);
            // Observable address escape prevents allocation/deallocation elision.
            observed = reinterpret_cast<std::uintptr_t>(p);
            allocator.deallocate(p, bytes);
        }
        const auto ns = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        if (repeat != 0) { samples.push_back(ns / operations); }
    }
    std::sort(samples.begin(), samples.end());
    std::cout << "backend,path,bytes,repetitions,operations,min_ns,median_ns,max_ns\n"
              << backend << ',' << path << ',' << bytes << ',' << repetitions << ','
              << operations << ',' << samples.front() << ',' << samples[samples.size()/2]
              << ',' << samples.back() << '\n';
}

void footprint(Backend backend, const std::string& name, StatisticsMode mode, bool dedicated) {
    std::unique_ptr<Memory> heap;
    if (dedicated) { heap.reset(new Memory(Memory::heap(backend, mode))); }
    else { Memory::configure_global(backend, mode); }
    auto& memory = dedicated ? *heap : Memory::global(backend);
    constexpr std::size_t count = 16384, bytes = 4096;
    std::vector<void*> pointers(count);
    const auto baseline = rss();
    for (auto& p : pointers) {
        p = memory.allocate(bytes, 16);
        std::memset(p, 1, bytes);
    }
    const auto live = rss();
    if (mode == StatisticsMode::Basic && memory.statistics()->live_bytes != count * bytes) {
        throw std::runtime_error("footprint request accounting mismatch");
    }
    for (auto p : pointers) { memory.deallocate(p, bytes, 16); }
    const auto freed = rss();
    if (dedicated) { heap->collect(); }
    const auto collected = rss();
    std::cout << "backend,statistics,heap,requested_bytes,baseline_rss,live_rss,freed_rss,collected_rss,peak_rss\n"
              << name << ',' << (mode == StatisticsMode::Basic ? "basic" : "disabled")
              << ',' << (dedicated ? "yes" : "no") << ',' << count * bytes << ','
              << baseline << ',' << live << ',' << freed << ',' << collected << ','
              << peak_rss() << '\n';
}

// A deterministic retention probe, not an isolated external-fragmentation rate.
// Keep sparse long-lived blocks while rotating the sizes of short-lived blocks.
template<class A>
void pressure(A& allocator, Backend backend, const std::string& name,
              const std::string& path) {
    struct Slot {
        void* pointer = nullptr;
        std::size_t bytes = 0;
        unsigned char pattern = 0;
    };
    constexpr std::size_t count = 16384;
    constexpr std::array<std::size_t, 10> sizes{
        17, 33, 65, 129, 257, 513, 1025, 2049, 4097, 8193};
    std::vector<Slot> slots(count);
    std::uint32_t random = 0xC0FFEE;
    std::uint64_t requested = 0;
    std::uint64_t maximum_requested = 0;
    // Warm the allocation path before taking a baseline, equally for all paths.
    for (unsigned i = 0; i < 128; ++i) {
        void* pointer = allocator.allocate(64);
        if (!pointer) { throw std::bad_alloc(); }
        static_cast<volatile unsigned char*>(pointer)[0] = 1;
        observed = reinterpret_cast<std::uintptr_t>(pointer);
        allocator.deallocate(pointer, 64);
    }
    const auto baseline = rss();
    std::cout << "backend,path,phase,slots,live_blocks,requested_bytes,usable_bytes,"
                 "baseline_rss,rss,peak_rss,max_requested_bytes,events,elapsed_ns\n";
    const auto snapshot = [&](const char* phase, std::size_t events, double elapsed) {
        std::size_t live = 0;
        std::uint64_t capacity = 0;
        for (const auto& slot : slots) {
            if (!slot.pointer) { continue; }
            ++live;
            const auto* bytes = static_cast<const unsigned char*>(slot.pointer);
            if (bytes[0] != slot.pattern || bytes[slot.bytes - 1] != slot.pattern) {
                throw std::runtime_error("pressure probe content mismatch");
            }
            if (reinterpret_cast<std::uintptr_t>(slot.pointer) % 16 != 0) {
                throw std::runtime_error("pressure probe alignment mismatch");
            }
            if (backend != Backend::Standard) {
                const auto usable = usable_bytes(backend, slot.pointer);
                if (usable < slot.bytes) { throw std::runtime_error("invalid usable size"); }
                capacity += usable;
            }
        }
        if constexpr (std::is_same_v<A, Unified>) {
            if (const auto stats = allocator.memory.statistics()) {
                if (stats->live_bytes != requested) {
                    throw std::runtime_error("pressure probe accounting mismatch");
                }
            }
        }
        std::cout << name << ',' << path << ',' << phase << ',' << count << ','
                  << live << ',' << requested << ',';
        if (backend != Backend::Standard) { std::cout << capacity; }
        std::cout << ',' << baseline << ',' << rss() << ',' << peak_rss() << ','
                  << maximum_requested << ',' << events << ',' << elapsed << '\n';
    };
    const auto fill = [&] {
        std::size_t events = 0;
        const auto start = Clock::now();
        for (auto& slot : slots) {
            if (slot.pointer) { continue; }
            random = random * 1664525u + 1013904223u;
            slot.bytes = sizes[(random >> 16) % sizes.size()];
            slot.pattern = static_cast<unsigned char>(random >> 24);
            slot.pointer = allocator.allocate(slot.bytes);
            if (!slot.pointer) { throw std::bad_alloc(); }
            auto* bytes = static_cast<volatile unsigned char*>(slot.pointer);
            for (std::size_t offset = 0; offset < slot.bytes; offset += 4096) {
                bytes[offset] = slot.pattern;
            }
            bytes[slot.bytes - 1] = slot.pattern;
            requested += slot.bytes;
            ++events;
        }
        maximum_requested = (std::max)(maximum_requested, requested);
        return std::pair{events, std::chrono::duration<double, std::nano>(
            Clock::now() - start).count()};
    };
    const auto release = [&](bool keep_anchors) {
        std::size_t events = 0;
        const auto start = Clock::now();
        for (std::size_t i = 0; i < slots.size(); ++i) {
            auto& slot = slots[i];
            if (!slot.pointer || (keep_anchors && i % 16 == 0)) { continue; }
            allocator.deallocate(slot.pointer, slot.bytes);
            requested -= slot.bytes;
            slot = {};
            ++events;
        }
        return std::pair{events, std::chrono::duration<double, std::nano>(
            Clock::now() - start).count()};
    };
    auto duration = fill();
    snapshot("dense", duration.first, duration.second);
    duration = release(true);
    snapshot("sparse", duration.first, duration.second);
    for (unsigned cycle = 0; cycle < 8; ++cycle) {
        duration = fill();
        if (cycle == 7) { snapshot("churn_dense", duration.first, duration.second); }
        duration = release(true);
    }
    snapshot("churn_sparse", duration.first, duration.second);
    duration = release(false);
    snapshot("freed", duration.first, duration.second);
    if (requested != 0) { throw std::runtime_error("pressure probe leaked requests"); }
}

void heap_operations(Backend backend, const std::string& name) {
    Memory heap = Memory::heap(backend);
    auto block = heap.make_block(64);
    for (auto operation : {"owns", "collect", "reset"}) {
        const unsigned count = std::string(operation) == "owns" ? 200000 : 2000;
        if (std::string(operation) == "reset") { block.resize(0); }
        std::vector<double> samples;
        const auto measure = [&](auto&& function) {
            for (unsigned repeat = 0; repeat < 9; ++repeat) {
                const auto start = Clock::now();
                for (unsigned i = 0; i < count; ++i) { function(); }
                samples.push_back(std::chrono::duration<double, std::nano>(
                    Clock::now() - start).count()/count);
            }
        };
        if (std::string(operation) == "owns") {
            measure([&] {
                if (!heap.owns(block.data())) { throw std::runtime_error("benchmark ownership"); }
            });
        } else if (std::string(operation) == "collect") {
            measure([&] { heap.collect(); });
        } else {
            measure([&] { heap.allocate(64); heap.reset(); });
        }
        std::sort(samples.begin(), samples.end());
        std::cout << name << ',' << operation << ',' << count << ',' << samples[4] << '\n';
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 4) { throw std::invalid_argument("backend workload path [bytes/heap] required"); }
        const std::string name = argv[1], workload = argv[2], path = argv[3];
        if (path != "native" && path != "disabled" && path != "basic") {
            throw std::invalid_argument("path must be native, disabled or basic");
        }
        Backend backend;
        if (name == "standard") { backend = Backend::Standard; }
        else if (name == "mimalloc") { backend = Backend::Mimalloc; }
        else if (name == "jemalloc") { backend = Backend::Jemalloc; }
        else { throw std::invalid_argument("unknown backend"); }
        if (!available(backend)) { throw std::runtime_error("backend unavailable"); }
        const auto mode = path == "basic" ? StatisticsMode::Basic : StatisticsMode::Disabled;
        if (workload == "environment") { environment(); }
        else if (workload == "footprint") { footprint(backend, name, mode, argc > 4 && std::string(argv[4]) == "heap"); }
        else if (workload == "heap") { heap_operations(backend, name); }
        else if (workload == "latency" || workload == "pressure" || workload == "tails" ||
                 workload == "scaling" || workload == "handoff") {
            const auto bytes = argc > 4 ? std::stoull(argv[4]) : 64;
            if (bytes == 0) { throw std::invalid_argument("nonzero benchmark size required"); }
            const auto measure = [&](auto& allocator) {
                if (workload == "pressure") { pressure(allocator, backend, name, path); }
                else if (workload == "tails") { tails(allocator, name, path, bytes); }
                else if (workload == "scaling" || workload == "handoff") {
                    scaling(allocator, name, path, bytes,
                            argc > 5 ? static_cast<unsigned>(std::stoul(argv[5])) : 1,
                            workload == "handoff");
                }
                else { latency(allocator, name, path, bytes); }
            };
            if (path != "native") { Unified allocator(backend, mode); measure(allocator); }
            else if (backend == Backend::Standard) { NativeStandard allocator; measure(allocator); }
#ifdef UNIMEMORY_BENCH_MIMALLOC
            else if (backend == Backend::Mimalloc) { NativeMimalloc allocator; measure(allocator); }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
            else if (backend == Backend::Jemalloc) { NativeJemalloc allocator; measure(allocator); }
#endif
            else { throw std::runtime_error("native benchmark unavailable"); }
        } else { throw std::invalid_argument("unknown workload"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
