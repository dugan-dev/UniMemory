#include "backend.h"

#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <new>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

#ifdef UNIMEMORY_BENCH_MIMALLOC
#include <mimalloc.h>
#if MI_MALLOC_VERSION < 30403
#error "Backend diagnostics require the existing mimalloc 3.4.3 interface"
#endif
#endif

#ifdef UNIMEMORY_BENCH_JEMALLOC
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4068)
#endif
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif

namespace unimem_diagnostics {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t alignment = 16;
constexpr std::size_t block_bytes = 65536;
constexpr std::size_t block_count = 1024;
constexpr std::size_t workload_bytes = block_bytes * block_count;
static_assert(workload_bytes == 64 * 1024 * 1024);

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void quoted(std::ostream& out, const std::string& value) {
    out << '"';
    for (const char c : value) {
        if (c == '"') { out << '"'; }
        out << c;
    }
    out << '"';
}

struct Reporter {
    std::ostream& out;
    const std::string& backend;
    const std::string& variant;

    template<class Value>
    void row(const std::string& suffix, std::size_t bytes,
             std::size_t operations, double seconds, Value value,
             const char* unit, std::uint64_t checksum = 0) const {
        out << "backend," << backend << ',' << variant << suffix << ','
            << bytes << ",1," << operations << ',' << std::setprecision(17)
            << seconds << ',' << value << ',' << unit << ',' << checksum << '\n';
    }

    void text(const std::string& suffix, const std::string& value) const {
        out << "backend," << backend << ',' << variant << suffix
            << ",0,1,0,0,";
        quoted(out, value);
        out << ",text,0\n";
    }

    void timing(const std::string& suffix, std::size_t bytes,
                std::size_t operations, Clock::duration elapsed,
                std::uint64_t checksum = 0) const {
        const double seconds = std::chrono::duration<double>(elapsed).count();
        row(suffix, bytes, operations, seconds,
            seconds * 1.0e9 / static_cast<double>(operations), "ns/op", checksum);
    }
};

std::size_t page_bytes() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    require(info.dwPageSize != 0, "GetSystemInfo returned a zero page size");
    return info.dwPageSize;
#else
    const auto value = sysconf(_SC_PAGESIZE);
    require(value > 0, "sysconf page-size query failed");
    return static_cast<std::size_t>(value);
#endif
}

struct ResidentSnapshot {
    std::uint64_t bytes;
    const char* source;
};

ResidentSnapshot resident_snapshot() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info{};
    info.cb = sizeof(info);
    require(GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)) != 0,
            "GetProcessMemoryInfo failed");
    return {static_cast<std::uint64_t>(info.WorkingSetSize), "working_set"};
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    require(task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                     reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS,
            "task_info resident query failed");
    return {static_cast<std::uint64_t>(info.resident_size), "mach_resident_size"};
#else
    // Current resident bytes, never ru_maxrss. The fallback is labeled because
    // Linux statm may have an approximate accounting value.
    std::ifstream rollup("/proc/self/smaps_rollup");
    if (rollup) {
        std::string line;
        while (std::getline(rollup, line)) {
            if (line.rfind("Rss:", 0) != 0) { continue; }
            unsigned long long kib = 0;
            char unit[8]{};
            require(std::sscanf(line.c_str(), "Rss: %llu %7s", &kib, unit) == 2 &&
                    std::string(unit) == "kB" &&
                    kib <= std::numeric_limits<std::uint64_t>::max() / 1024,
                    "invalid smaps_rollup RSS record");
            return {static_cast<std::uint64_t>(kib) * 1024, "smaps_rollup_rss"};
        }
        throw std::runtime_error("smaps_rollup has no RSS record");
    }
    std::ifstream statm("/proc/self/statm");
    std::uint64_t total = 0;
    std::uint64_t resident = 0;
    require(static_cast<bool>(statm >> total >> resident), "statm RSS query failed");
    const auto page = page_bytes();
    require(resident <= std::numeric_limits<std::uint64_t>::max() / page,
            "statm RSS multiplication overflow");
    return {resident * page, "statm_resident_approximate"};
#endif
}

void report_resident(const Reporter& report, const std::string& stage,
                     std::uint64_t checksum) {
    const auto snapshot = resident_snapshot(); // All RSS work stays outside timers.
    report.row("_rss_" + stage, workload_bytes, block_count, 0,
               snapshot.bytes, "bytes", checksum);
}

void* nonnull(void* pointer) {
    if (pointer == nullptr) { throw std::bad_alloc(); }
    return pointer;
}

#ifdef UNIMEMORY_BENCH_JEMALLOC
template<class Value>
Value read_ctl(const char* name) {
    Value value{};
    std::size_t length = sizeof(value);
    const int error = je_mallctl(name, &value, &length, nullptr, 0);
    if (error != 0 || length != sizeof(value)) {
        throw std::runtime_error(std::string("jemalloc read failed: ") + name +
                                 " error=" + std::to_string(error) +
                                 " length=" + std::to_string(length));
    }
    return value;
}

void command_ctl(const char* name) {
    const int error = je_mallctl(name, nullptr, nullptr, nullptr, 0);
    if (error != 0) {
        throw std::runtime_error(std::string("jemalloc command failed: ") +
                                 name + " error=" + std::to_string(error));
    }
}

void arena_command(unsigned index, const char* command) {
    char name[64]{};
    const auto count = std::snprintf(name, sizeof(name), "arena.%u.%s", index, command);
    require(count > 0 && static_cast<std::size_t>(count) < sizeof(name),
            "jemalloc arena command name overflow");
    command_ctl(name);
}

unsigned lookup_arena(const void* pointer) {
    unsigned index = 0;
    std::size_t length = sizeof(index);
    void* allocation = const_cast<void*>(pointer);
    const int error = je_mallctl("arenas.lookup", &index, &length,
                                &allocation, sizeof(allocation));
    require(error == 0 && length == sizeof(index), "jemalloc arenas.lookup failed");
    return index;
}

void report_arena_policy(const Reporter& report, const void* pointer) {
    const unsigned index = lookup_arena(pointer);
    using SignedSize = std::make_signed_t<std::size_t>;
    char name[64]{};
    for (const auto* suffix : {"dirty_decay_ms", "muzzy_decay_ms"}) {
        const int count = std::snprintf(name, sizeof(name), "arena.%u.%s", index, suffix);
        require(count > 0 && static_cast<std::size_t>(count) < sizeof(name),
                "jemalloc decay name overflow");
        report.row(std::string("_active_arena_") + suffix, 0, 0, 0,
                   read_ctl<SignedSize>(name), "ms");
    }
    report.row("_active_arena_index", 0, 0, 0, index, "index");
}
#endif

void global_collect(unimem::Backend backend) {
#ifdef UNIMEMORY_BENCH_MIMALLOC
    if (backend == unimem::Backend::Mimalloc) { mi_collect(true); return; }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    if (backend == unimem::Backend::Jemalloc) {
        command_ctl("thread.tcache.flush");
        arena_command(MALLCTL_ARENAS_ALL, "purge");
        return;
    }
#endif
    (void)backend;
    throw std::invalid_argument("backend collection is unavailable");
}

// Each adapter keeps allocation, free, alignment and dedicated policy identical.
// Basic UniMemory tracking is disabled so it is not an additional variable.
struct ApiAllocator {
    const unimem::Backend backend;
    const bool dedicated;
    std::unique_ptr<unimem::Memory> heap;
    unimem::Memory* memory = nullptr;

    ApiAllocator(unimem::Backend selected, bool is_heap)
        : backend(selected), dedicated(is_heap) {
        if (dedicated) {
            heap.reset(new unimem::Memory(unimem::Memory::heap(
                backend, unimem::StatisticsMode::Disabled)));
            memory = heap.get();
        } else {
            unimem::Memory::configure_global(backend, unimem::StatisticsMode::Disabled);
            memory = &unimem::Memory::global(backend);
        }
    }
    void* allocate(std::size_t bytes) { return memory->allocate(bytes, alignment); }
    void deallocate(void* pointer, std::size_t bytes) {
        memory->deallocate(pointer, bytes, alignment);
    }
    void collect() {
        if (dedicated) { memory->collect(); }
        else { global_collect(backend); }
    }
    bool owns(const void* pointer) const { return memory->owns(pointer); }
    void reset() { memory->reset(); }
    void destroy() {
        require(dedicated, "global Memory has no explicit destruction control");
        heap.reset();
        memory = nullptr;
    }
};

#ifdef UNIMEMORY_BENCH_MIMALLOC
struct NativeMimalloc {
    const bool dedicated;
    mi_heap_t* heap = nullptr;

    explicit NativeMimalloc(bool is_heap) : dedicated(is_heap) {
        if (dedicated) { heap = static_cast<mi_heap_t*>(nonnull(mi_heap_new())); }
    }
    ~NativeMimalloc() { if (heap != nullptr) { mi_heap_destroy(heap); } }
    void* allocate(std::size_t bytes) {
        return nonnull(dedicated ? mi_heap_malloc_aligned(heap, bytes, alignment)
                                 : mi_malloc_aligned(bytes, alignment));
    }
    void deallocate(void* pointer, std::size_t) { mi_free(pointer); }
    void collect() {
        if (dedicated) { mi_heap_collect(heap, true); }
        else { mi_collect(true); }
    }
    bool owns(const void* pointer) const {
        return pointer != nullptr && mi_heap_contains(heap, pointer);
    }
    void reset() {
        auto* next = static_cast<mi_heap_t*>(nonnull(mi_heap_new()));
        // Exactly matches UniMemory's transactional reset: new, destroy, assign.
        mi_heap_destroy(heap);
        heap = next;
    }
    void destroy() { mi_heap_destroy(heap); heap = nullptr; }
};
#endif

#ifdef UNIMEMORY_BENCH_JEMALLOC
struct NativeJemalloc {
    const bool dedicated;
    const bool no_tcache;
    unsigned index = 0;
    bool alive = false;

    explicit NativeJemalloc(bool is_heap, bool disable_tcache = false)
        : dedicated(is_heap), no_tcache(disable_tcache || is_heap) {
        if (!dedicated) { return; }
        std::size_t length = sizeof(index);
        const int error = je_mallctl("arenas.create", &index, &length, nullptr, 0);
        require(error == 0 && length == sizeof(index), "jemalloc arenas.create failed");
        alive = true;
        constexpr unsigned max_index = (1u << (sizeof(int) * CHAR_BIT - 21)) - 2u;
        if (index > max_index) {
            destroy();
            throw std::runtime_error("jemalloc arena index exceeds MALLOCX_ARENA range");
        }
    }
    ~NativeJemalloc() {
        if (alive) {
            // Cleanup while unwinding must not replace the first diagnostic
            // failure. Successful paths call checked destroy() explicitly.
            try { destroy(); } catch (...) {}
        }
    }
    int flags() const {
        return MALLOCX_ALIGN(alignment) |
               (dedicated ? MALLOCX_ARENA(index) : 0) |
               (no_tcache ? MALLOCX_TCACHE_NONE : 0);
    }
    void* allocate(std::size_t bytes) { return nonnull(je_mallocx(bytes, flags())); }
    void deallocate(void* pointer, std::size_t) {
        je_dallocx(pointer, no_tcache ? MALLOCX_TCACHE_NONE : 0);
    }
    void collect() {
        if (dedicated) { arena_command(index, "purge"); }
        else { global_collect(unimem::Backend::Jemalloc); }
    }
    bool owns(const void* pointer) const {
        return pointer != nullptr && lookup_arena(pointer) == index;
    }
    void reset() { arena_command(index, "reset"); }
    void destroy() {
        require(alive, "jemalloc arena is already destroyed");
        arena_command(index, "destroy");
        alive = false;
    }
};
#endif

void environment(const Reporter& report, unimem::Backend backend) {
    report.row("_page_bytes", 0, 0, 0, page_bytes(), "bytes");
    report.text("_rss_source", resident_snapshot().source);
    report.row("_payload_bytes", 0, 0, 0, workload_bytes, "bytes");
    report.row("_alignment", 0, 0, 0, alignment, "bytes");
    report.row("_delay_before_second_collect", 0, 0, 0, 100, "ms");
    report.row("_basic_statistics_enabled", 0, 0, 0, 0, "bool");
#ifdef UNIMEMORY_BENCH_MIMALLOC
    if (backend == unimem::Backend::Mimalloc) {
        report.row("_runtime_version", 0, 0, 0, mi_version(), "version");
        report.row("_header_version", 0, 0, 0, MI_MALLOC_VERSION, "version");
        report.row("_redirected", 0, 0, 0, mi_is_redirected() ? 1 : 0, "bool");
        report.row("_purge_delay", 0, 0, 0, mi_option_get(mi_option_purge_delay), "ms");
        report.row("_purge_decommits", 0, 0, 0,
                   mi_option_get(mi_option_purge_decommits), "bool");
        report.row("_arena_purge_mult", 0, 0, 0,
                   mi_option_get(mi_option_arena_purge_mult), "multiplier");
        report.row("_arena_eager_commit", 0, 0, 0,
                   mi_option_get(mi_option_arena_eager_commit), "mode");
        report.row("_arena_reserve", 0, 0, 0,
                   mi_option_get_size(mi_option_arena_reserve), "bytes");
        report.row("_page_commit_on_demand", 0, 0, 0,
                   mi_option_get(mi_option_page_commit_on_demand), "bool");
        report.row("_allow_thp", 0, 0, 0, mi_option_get(mi_option_allow_thp), "bool");
        return;
    }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    if (backend == unimem::Backend::Jemalloc) {
        const auto* version = read_ctl<const char*>("version");
        require(version != nullptr, "jemalloc returned a null version");
        report.text("_runtime_version", version);
        report.text("_header_version", JEMALLOC_VERSION);
        report.row("_config_stats", 0, 0, 0, read_ctl<bool>("config.stats") ? 1 : 0, "bool");
        report.row("_opt_tcache", 0, 0, 0, read_ctl<bool>("opt.tcache") ? 1 : 0, "bool");
        report.row("_opt_retain", 0, 0, 0, read_ctl<bool>("opt.retain") ? 1 : 0, "bool");
        using SignedSize = std::make_signed_t<std::size_t>;
        report.row("_arenas_dirty_decay_ms", 0, 0, 0,
                   read_ctl<SignedSize>("arenas.dirty_decay_ms"), "ms");
        report.row("_arenas_muzzy_decay_ms", 0, 0, 0,
                   read_ctl<SignedSize>("arenas.muzzy_decay_ms"), "ms");
        return;
    }
#endif
    (void)backend;
    throw std::invalid_argument("backend native diagnostics were not compiled");
}

template<class Allocator>
void retention(const Reporter& report, Allocator& allocator,
               unimem::Backend backend) {
    std::array<void*, block_count> blocks{};
    const auto page = page_bytes();
    require(page <= block_bytes, "diagnostic block cannot cover one OS page");
    report_resident(report, "baseline", 0);
    std::uint64_t checksum = 0;
    Clock::duration allocation_time{};
    Clock::duration free_time{};
    Clock::duration collect_time{};
    Clock::duration delayed_collect_time{};
    Clock::duration destroy_time{};
    try {
        auto started = Clock::now();
        for (auto& block : blocks) { block = allocator.allocate(block_bytes); }
        allocation_time = Clock::now() - started;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            require(reinterpret_cast<std::uintptr_t>(blocks[i]) % alignment == 0,
                    "backend violated the fixed alignment");
            auto* bytes = static_cast<volatile unsigned char*>(blocks[i]);
            // Offset zero, every OS-page stride, and the final byte together
            // touch every physical page even if the allocation is not page aligned.
            for (std::size_t offset = 0; offset < block_bytes; offset += page) {
                bytes[offset] = static_cast<unsigned char>((i + offset / page) & 255);
                checksum += bytes[offset];
            }
            bytes[block_bytes - 1] = static_cast<unsigned char>((i + 17) & 255);
            checksum += bytes[block_bytes - 1];
        }
        report_resident(report, "live", checksum);
#ifdef UNIMEMORY_BENCH_JEMALLOC
        if (backend == unimem::Backend::Jemalloc) { report_arena_policy(report, blocks[0]); }
#else
        (void)backend;
#endif
        started = Clock::now();
        for (auto& block : blocks) {
            allocator.deallocate(block, block_bytes);
            block = nullptr;
        }
        free_time = Clock::now() - started;
        report_resident(report, "freed", checksum);
        started = Clock::now();
        allocator.collect();
        collect_time = Clock::now() - started;
        report_resident(report, "collected", checksum);
        // An observation of delayed reclamation, never a leak verdict. The wait
        // is intentionally outside both collection timers and all hot loops.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        started = Clock::now();
        allocator.collect();
        delayed_collect_time = Clock::now() - started;
        report_resident(report, "delayed_collect", checksum);
        if (allocator.dedicated) {
            started = Clock::now();
            allocator.destroy(); // Every allocation was freed before destruction.
            destroy_time = Clock::now() - started;
            report_resident(report, "destroy", checksum);
        }
    } catch (...) {
        for (auto& block : blocks) {
            if (block != nullptr) { allocator.deallocate(block, block_bytes); block = nullptr; }
        }
        throw;
    }
    // Emit timing rows only after all retention snapshots. RSS reporting is
    // outside allocation/free timing and uses the same sequence in each variant.
    report.timing("_allocate", block_bytes, block_count, allocation_time, checksum);
    report.timing("_free", block_bytes, block_count, free_time, checksum);
    const std::string collect_label = allocator.dedicated ? "_collect" : "_backend_collect";
    report.timing(collect_label, workload_bytes, 1, collect_time, checksum);
    report.timing(collect_label + "_after_100ms", workload_bytes, 1,
                  delayed_collect_time, checksum);
    if (allocator.dedicated) { report.timing("_destroy", workload_bytes, 1, destroy_time, checksum); }
}

template<class Allocator>
void controls(const Reporter& report, Allocator& allocator, std::size_t iterations) {
    constexpr std::size_t bytes = 64;
    std::uint64_t checksum = 0;
    auto started = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        void* pointer = allocator.allocate(bytes);
        auto* cell = static_cast<volatile unsigned char*>(pointer);
        *cell = static_cast<unsigned char>(i & 255);
        checksum += *cell;
        allocator.deallocate(pointer, bytes);
    }
    report.timing("_pair_64", bytes, iterations * 2, Clock::now() - started, checksum);
    if (!allocator.dedicated) { return; }
    void* pointer = allocator.allocate(bytes);
    try {
        require(!allocator.owns(nullptr), "empty ownership query should be false");
        started = Clock::now();
        std::uint64_t own_count = 0;
        for (std::size_t i = 0; i < iterations; ++i) {
            own_count += allocator.owns(pointer) ? 1 : 0;
        }
        const auto elapsed = Clock::now() - started;
        require(own_count == iterations, "ownership query rejected a live allocation");
        report.timing("_owns", bytes, iterations, elapsed, own_count);
    } catch (...) {
        allocator.deallocate(pointer, bytes);
        throw;
    }
    allocator.deallocate(pointer, bytes);
    pointer = nullptr;
    started = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) { allocator.collect(); }
    report.timing("_collect_empty", 0, iterations, Clock::now() - started, iterations);
    started = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) { allocator.reset(); }
    const auto reset_time = Clock::now() - started;
    // Verify usability after reset using a new allocation, never an invalidated
    // pointer. No objects or allocations were live during any reset.
    pointer = allocator.allocate(bytes);
    const bool owned = allocator.owns(pointer);
    allocator.deallocate(pointer, bytes);
    require(owned, "heap failed allocation ownership after reset");
    report.timing("_reset_empty", 0, iterations, reset_time, iterations);
    allocator.destroy();
}

#ifdef UNIMEMORY_BENCH_JEMALLOC
void epoch_refresh() {
    std::uint64_t epoch = 1;
    const int error = je_mallctl("epoch", nullptr, nullptr, &epoch, sizeof(epoch));
    require(error == 0, "jemalloc epoch refresh failed");
}

struct ProcessStatistics { std::uint64_t allocated; std::uint64_t resident; };

ProcessStatistics native_statistics(bool refresh) {
    if (refresh) {
        require(read_ctl<bool>("config.stats"), "jemalloc statistics are disabled");
        epoch_refresh();
    }
    return {read_ctl<std::size_t>("stats.allocated"),
            read_ctl<std::size_t>("stats.resident")};
}

ProcessStatistics api_statistics(unimem::Memory& memory) {
    const auto stats = memory.backend_statistics();
    require(stats.has_value() && stats->scope == unimem::BackendStatisticsScope::Process &&
            stats->allocated_bytes.has_value() && stats->resident_bytes.has_value(),
            "UniMemory detailed jemalloc statistics are unavailable or incomplete");
    return {*stats->allocated_bytes, *stats->resident_bytes};
}

void statistics(const Reporter& report, std::size_t iterations) {
    require(read_ctl<bool>("config.stats"), "jemalloc statistics are disabled");
    // Only api_stats initializes a UniMemory global object. Both paths use the
    // same explicit global jemalloc backend and one aligned 64-byte live block.
    std::unique_ptr<ApiAllocator> api;
    if (report.variant == "api_stats") {
        api = std::make_unique<ApiAllocator>(unimem::Backend::Jemalloc, false);
    }
    NativeJemalloc native(false);
    void* pointer = api ? api->allocate(64) : native.allocate(64);
    auto* byte = static_cast<volatile unsigned char*>(pointer);
    *byte = 73;
    std::uint64_t checksum = *byte;
    ProcessStatistics last{};
    try {
        last = native_statistics(true); // Equal untimed warm-up.
        const auto started = Clock::now();
        for (std::size_t i = 0; i < iterations; ++i) {
            if (report.variant == "epoch_only") {
                epoch_refresh();
                ++checksum;
            } else {
                last = api ? api_statistics(*api->memory)
                           : native_statistics(report.variant == "native_stats");
                checksum += last.allocated + last.resident;
            }
        }
        const auto elapsed = Clock::now() - started;
        report.timing("_query", 64, iterations, elapsed, checksum);
        if (report.variant == "epoch_only") { last = native_statistics(false); }
        report.row("_allocated", 64, 1, 0, last.allocated, "bytes", checksum);
        report.row("_resident", 64, 1, 0, last.resident, "bytes", checksum);
    } catch (...) {
        if (api) { api->deallocate(pointer, 64); }
        else { native.deallocate(pointer, 64); }
        throw;
    }
    if (api) { api->deallocate(pointer, 64); }
    else { native.deallocate(pointer, 64); }
}
#endif

template<class Native>
void allocation_experiment(const Reporter& report, unimem::Backend backend,
                           std::size_t iterations, bool dedicated) {
    if (report.variant.rfind("api_", 0) == 0) {
        ApiAllocator allocator(backend, dedicated);
        retention(report, allocator, backend);
        ApiAllocator basic(backend, dedicated);
        controls(report, basic, iterations);
    } else {
        Native allocator(dedicated);
        retention(report, allocator, backend);
        Native basic(dedicated);
        controls(report, basic, iterations);
    }
}
}

void run_backend(std::ostream& out, const std::string& variant,
                 const std::string& backend, std::size_t iterations) {
    if (iterations == 0 || iterations > std::numeric_limits<std::size_t>::max() / 2) {
        throw std::invalid_argument("backend iterations must be positive and safely countable");
    }
    const bool dedicated = variant == "native_heap" || variant == "api_heap";
    const bool global = variant == "native_global" || variant == "api_global";
    const bool stats = variant == "native_stats" || variant == "api_stats" ||
                       variant == "epoch_only" || variant == "native_cached_stats";
    const Reporter report{out, backend, variant};
#ifdef UNIMEMORY_BENCH_MIMALLOC
    if (backend == "mimalloc") {
        if (!dedicated && !global) {
            throw std::invalid_argument("unsupported mimalloc diagnostic variant: " + variant);
        }
        environment(report, unimem::Backend::Mimalloc);
        allocation_experiment<NativeMimalloc>(report, unimem::Backend::Mimalloc,
                                              iterations, dedicated);
        return;
    }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    if (backend == "jemalloc") {
        if (!dedicated && !global && !stats && variant != "native_no_tcache") {
            throw std::invalid_argument("unsupported jemalloc diagnostic variant: " + variant);
        }
        environment(report, unimem::Backend::Jemalloc);
        if (stats) { statistics(report, iterations); return; }
        if (variant == "native_no_tcache") {
            NativeJemalloc allocator(false, true);
            retention(report, allocator, unimem::Backend::Jemalloc);
            NativeJemalloc basic(false, true);
            controls(report, basic, iterations);
        } else {
            allocation_experiment<NativeJemalloc>(report, unimem::Backend::Jemalloc,
                                                  iterations, dedicated);
        }
        return;
    }
#endif
    (void)dedicated;
    (void)global;
    (void)stats;
    (void)report;
    throw std::invalid_argument("unsupported or uncompiled diagnostic backend: " + backend);
}
}
