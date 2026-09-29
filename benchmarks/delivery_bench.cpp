#include <unimem/memory.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
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
        else if (workload == "latency" || workload == "pressure") {
            const auto bytes = argc > 4 ? std::stoull(argv[4]) : 64;
            if (bytes == 0) { throw std::invalid_argument("nonzero benchmark size required"); }
            const auto measure = [&](auto& allocator) {
                if (workload == "pressure") { pressure(allocator, backend, name, path); }
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
