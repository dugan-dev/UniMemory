#include "statistics.h"

#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#ifdef UNIMEMORY_BENCH_MIMALLOC
#include <mimalloc.h>
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>
#endif

namespace unimem_diagnostics {
namespace {

constexpr std::size_t bytes = 64;
constexpr std::size_t alignment = 16;
// Cover the coherence-line size on the tested AMD64 and Apple M1 runners.
constexpr std::size_t padding = 128;
constexpr std::size_t warm_iterations = 256;
using Clock = std::chrono::steady_clock;

void* checked_native_allocation(void* pointer) {
    if (pointer == nullptr) { throw std::bad_alloc(); }
    return pointer;
}

struct NativeStandard {
    void* allocate() const {
        // Match src/backend_system.cpp, including its nothrow allocation form.
        return checked_native_allocation(
            ::operator new(bytes, std::align_val_t{alignment}, std::nothrow));
    }
    void deallocate(void* pointer) const noexcept {
        ::operator delete(pointer, std::align_val_t{alignment});
    }
};

#ifdef UNIMEMORY_BENCH_MIMALLOC
struct NativeMimalloc {
    void* allocate() const {
        return checked_native_allocation(mi_malloc_aligned(bytes, alignment));
    }
    void deallocate(void* pointer) const noexcept { mi_free(pointer); }
};
#endif

#ifdef UNIMEMORY_BENCH_JEMALLOC
struct NativeJemalloc {
    NativeJemalloc() {
        // Match production's main-thread jemalloc bootstrap before workers.
        // Windows jemalloc TSD initialization can race on simultaneous first use.
        const char* version = nullptr;
        std::size_t length = sizeof(version);
        if (je_mallctl("version", &version, &length, nullptr, 0) != 0 ||
            length != sizeof(version) || version == nullptr) {
            throw std::runtime_error("statistics diagnostic jemalloc initialization failed");
        }
    }
    void* allocate() const {
        return checked_native_allocation(je_mallocx(bytes, MALLOCX_ALIGN(alignment)));
    }
    void deallocate(void* pointer) const noexcept { je_dallocx(pointer, 0); }
};
#endif

struct ActualMemory {
    unimem::Memory* memory;
    void* allocate() const { return memory->allocate(bytes, alignment); }
    void deallocate(void* pointer) const noexcept {
        memory->deallocate(pointer, bytes, alignment);
    }
};

struct Snapshot {
    std::uint64_t allocations = 0;
    std::uint64_t deallocations = 0;
    std::uint64_t reallocations = 0;
    std::uint64_t live_bytes = 0;
    std::uint64_t peak_live_bytes = 0;
};

struct alignas(padding) WorkerResult {
    double seconds = 0;
    Clock::time_point started;
    Clock::time_point finished;
    std::uint64_t checksum = 0;
    Snapshot counters;
    std::exception_ptr error;
};
static_assert(alignof(WorkerResult) == padding);
static_assert(sizeof(WorkerResult) % padding == 0);

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

Clock::duration elapsed_span(std::span<const WorkerResult> workers) {
    require(!workers.empty(), "empty statistics timing window");
    auto first=workers.front().started;
    auto last=workers.front().finished;
    for (const auto& worker:workers) {
        require(worker.finished>=worker.started, "reversed statistics timing window");
        first=std::min(first,worker.started);
        last=std::max(last,worker.finished);
    }
    return last-first;
}

void verify_elapsed_span() {
    using namespace std::chrono_literals;
    std::array<WorkerResult,2> windows{};
    windows[0].started=Clock::time_point{};
    windows[0].finished=Clock::time_point{}+10ms;
    windows[1].started=Clock::time_point{}+100ms;
    windows[1].finished=Clock::time_point{}+110ms;
    // max(individual duration)=10ms would incorrectly turn serial work into
    // parallel throughput. The actual two-worker span is 110ms.
    require(elapsed_span(windows)==110ms,"staggered statistics timer regression");
    windows[1].started=Clock::time_point{}+5ms;
    windows[1].finished=Clock::time_point{}+15ms;
    require(elapsed_span(windows)==15ms,"overlapping statistics timer regression");
    require(elapsed_span(std::span<const WorkerResult>(windows).first(1))==10ms,
            "single-worker statistics timer regression");
}

void validate_full(const Snapshot& counters, std::uint64_t operations,
                   std::size_t peak_workers, bool peak_enabled = true) {
    require(counters.allocations == operations &&
                counters.deallocations == operations &&
                counters.reallocations == 0 && counters.live_bytes == 0,
            "statistics diagnostic counter totals mismatch");
    if (peak_enabled) {
        require(counters.peak_live_bytes >= bytes &&
                    counters.peak_live_bytes <= peak_workers * bytes,
                "statistics diagnostic peak outside valid bounds");
    } else {
        require(counters.peak_live_bytes == 0,
                "statistics diagnostic no-peak counter was updated");
    }
}

struct EmptyWorker {
    void allocation() const noexcept {}
    void deallocation() const noexcept {}
    Snapshot snapshot() const noexcept { return {}; }
};

struct NoCounters {
    EmptyWorker worker(std::size_t) const noexcept { return {}; }
    void validate(const std::vector<WorkerResult>&, std::uint64_t) const noexcept {}
};

struct PlainWorker {
    Snapshot* counters;
    void allocation() const noexcept {
        ++counters->allocations;
        counters->live_bytes += bytes;
        counters->peak_live_bytes =
            std::max(counters->peak_live_bytes, counters->live_bytes);
    }
    void deallocation() const noexcept {
        ++counters->deallocations;
        counters->live_bytes -= bytes;
    }
    Snapshot snapshot() const noexcept { return *counters; }
};

struct ThreadLocalPlain {
    PlainWorker worker(std::size_t) const noexcept {
        static thread_local Snapshot counters;
        counters = {};
        return {&counters};
    }
    void validate(const std::vector<WorkerResult>& results,
                  std::uint64_t worker_operations) const {
        for (const auto& result : results) {
            validate_full(result.counters, worker_operations, 1);
        }
    }
};

struct CompactAtomic {
    std::atomic<std::uint64_t> value{0};
};

struct alignas(padding) PaddedAtomic {
    std::atomic<std::uint64_t> value{0};
};
static_assert(sizeof(PaddedAtomic) == padding);

struct alignas(padding) PaddedPlain {
    std::uint64_t value = 0;
};
static_assert(sizeof(PaddedPlain) == sizeof(PaddedAtomic));
static_assert(alignof(PaddedPlain) == alignof(PaddedAtomic));

template<class Word>
struct alignas(padding) CounterLayout {
    Word allocations;
    Word deallocations;
    // Preserve the unused reallocation slot from the production field order.
    Word reallocations;
    Word live_bytes;
    Word peak_live_bytes;
};
using CompactCounters = CounterLayout<CompactAtomic>;
using PaddedCounters = CounterLayout<PaddedAtomic>;
using PaddedPlainCounters = CounterLayout<PaddedPlain>;
static_assert(sizeof(CompactCounters) == padding);
static_assert(offsetof(CompactCounters, peak_live_bytes) + sizeof(CompactAtomic) <= padding);
static_assert(sizeof(PaddedCounters) == 5 * padding);
static_assert(sizeof(PaddedPlainCounters) == sizeof(PaddedCounters));
static_assert(offsetof(PaddedPlainCounters, allocations) == offsetof(PaddedCounters, allocations));
static_assert(offsetof(PaddedPlainCounters, deallocations) == offsetof(PaddedCounters, deallocations));
static_assert(offsetof(PaddedPlainCounters, reallocations) == offsetof(PaddedCounters, reallocations));
static_assert(offsetof(PaddedPlainCounters, live_bytes) == offsetof(PaddedCounters, live_bytes));
static_assert(offsetof(PaddedPlainCounters, peak_live_bytes) == offsetof(PaddedCounters, peak_live_bytes));

template<class Counters>
Snapshot snapshot(const Counters& counters) noexcept {
    return {counters.allocations.value.load(std::memory_order_relaxed),
            counters.deallocations.value.load(std::memory_order_relaxed),
            counters.reallocations.value.load(std::memory_order_relaxed),
            counters.live_bytes.value.load(std::memory_order_relaxed),
            counters.peak_live_bytes.value.load(std::memory_order_relaxed)};
}

Snapshot snapshot(const PaddedPlainCounters& counters) noexcept {
    return {counters.allocations.value, counters.deallocations.value,
            counters.reallocations.value, counters.live_bytes.value,
            counters.peak_live_bytes.value};
}

enum class CounterFeatures {
    Full, NoPeak, LiveOnly, CountsOnly, StressFull, StressNoPeak
};

template<class Counters, CounterFeatures Features>
struct AtomicWorker {
    Counters* counters;
    void allocation() const noexcept {
        if constexpr (Features == CounterFeatures::StressFull ||
                      Features == CounterFeatures::StressNoPeak) {
            // Artificial sensitivity probe: both stress variants reset peak.
            // The paired full variant then exercises the production load/CAS
            // loop frequently; this is not production Basic peak semantics.
            counters->peak_live_bytes.value.store(0, std::memory_order_relaxed);
        }
        if constexpr (Features != CounterFeatures::LiveOnly) {
            counters->allocations.value.fetch_add(1, std::memory_order_relaxed);
        }
        if constexpr (Features != CounterFeatures::CountsOnly) {
            const auto live = counters->live_bytes.value.fetch_add(
                                  bytes, std::memory_order_relaxed) + bytes;
            if constexpr (Features == CounterFeatures::Full ||
                          Features == CounterFeatures::StressFull) {
                // Same relaxed peak load/CAS loop as production record_allocation.
                auto peak = counters->peak_live_bytes.value.load(
                    std::memory_order_relaxed);
                while (live > peak &&
                       !counters->peak_live_bytes.value.compare_exchange_weak(
                           peak, live, std::memory_order_relaxed)) {}
            } else {
                (void)live;
            }
        }
    }
    void deallocation() const noexcept {
        if constexpr (Features != CounterFeatures::LiveOnly) {
            counters->deallocations.value.fetch_add(1, std::memory_order_relaxed);
        }
        if constexpr (Features != CounterFeatures::CountsOnly) {
            counters->live_bytes.value.fetch_sub(bytes, std::memory_order_relaxed);
        }
    }
    // The coordinator reads these counters after every timed loop has ended.
    // An early-finishing worker must not add shared loads to another's timing.
    Snapshot snapshot() const noexcept { return {}; }
};

template<class Counters, CounterFeatures Features>
struct SharedAtomic {
    Counters counters;
    AtomicWorker<Counters, Features> worker(std::size_t) noexcept {
        return {&counters};
    }
    void validate(const std::vector<WorkerResult>& results,
                  std::uint64_t worker_operations) const {
        const auto final = snapshot(counters);
        const auto operations = worker_operations * results.size();
        if constexpr (Features == CounterFeatures::LiveOnly) {
            require(final.allocations == 0 && final.deallocations == 0 &&
                        final.reallocations == 0 && final.live_bytes == 0 &&
                        final.peak_live_bytes == 0,
                    "statistics diagnostic live-only counters mismatch");
        } else if constexpr (Features == CounterFeatures::CountsOnly) {
            validate_full(final, operations, results.size(), false);
        } else {
            validate_full(final, operations, results.size(),
                          Features == CounterFeatures::Full ||
                          Features == CounterFeatures::StressFull);
        }
    }
};

struct WorkerPaddedAtomic {
    std::unique_ptr<PaddedCounters[]> counters;
    explicit WorkerPaddedAtomic(std::size_t threads)
        : counters(std::make_unique<PaddedCounters[]>(threads)) {}
    AtomicWorker<PaddedCounters, CounterFeatures::Full> worker(std::size_t index) noexcept {
        return {&counters[index]};
    }
    void validate(const std::vector<WorkerResult>& results,
                  std::uint64_t worker_operations) const {
        for (std::size_t index = 0; index < results.size(); ++index) {
            validate_full(snapshot(counters[index]), worker_operations, 1);
        }
    }
};

struct PaddedPlainWorker {
    PaddedPlainCounters* counters;
    void allocation() const noexcept {
        ++counters->allocations.value;
        const auto live = counters->live_bytes.value + bytes;
        counters->live_bytes.value = live;
        const auto peak = counters->peak_live_bytes.value;
        if (live > peak) { counters->peak_live_bytes.value = live; }
    }
    void deallocation() const noexcept {
        ++counters->deallocations.value;
        counters->live_bytes.value -= bytes;
    }
    // Match the atomic worker: final counter reads happen after every loop.
    Snapshot snapshot() const noexcept { return {}; }
};

struct WorkerPaddedPlain {
    std::unique_ptr<PaddedPlainCounters[]> counters;
    explicit WorkerPaddedPlain(std::size_t threads)
        : counters(std::make_unique<PaddedPlainCounters[]>(threads)) {}
    PaddedPlainWorker worker(std::size_t index) noexcept {
        return {&counters[index]};
    }
    void validate(const std::vector<WorkerResult>& results,
                  std::uint64_t worker_operations) const {
        for (std::size_t index = 0; index < results.size(); ++index) {
            validate_full(snapshot(counters[index]), worker_operations, 1);
        }
    }
};

template<class Allocator, class Worker>
std::uint64_t allocation_pair(const Allocator& allocator, const Worker& counters,
                              std::size_t iteration, std::size_t worker) {
    void* pointer = allocator.allocate();
    counters.allocation();
    // Volatile accesses keep the identical allocation/content workload observable.
    auto* content = static_cast<volatile unsigned char*>(pointer);
    const auto value = static_cast<unsigned char>((iteration + worker) & 255);
    content[0] = value;
    content[bytes - 1] = static_cast<unsigned char>(value ^ 0x5a);
    const auto checksum = static_cast<std::uint64_t>(content[0]) + content[bytes - 1];
    allocator.deallocate(pointer);
    counters.deallocation();
    return checksum;
}

std::uint64_t expected_checksum(std::size_t iterations, std::size_t threads) {
    // A complete 256-element cycle sums each permutation of [0,255] twice.
    std::uint64_t checksum = (iterations / 256) * 65280ULL * threads;
    for (std::size_t worker = 0; worker < threads; ++worker) {
        for (std::size_t index = 0; index < iterations % 256; ++index) {
            const auto value = static_cast<unsigned char>((index + worker) & 255);
            checksum += value + static_cast<unsigned char>(value ^ 0x5a);
        }
    }
    return checksum;
}

struct Measurement {
    double seconds = 0;
    std::uint64_t checksum = 0;
};

template<class Allocator, class CounterFactory>
Measurement measure(const Allocator& allocator, CounterFactory& factory,
                    std::size_t iterations, std::size_t thread_count) {
    struct Gate {
        std::mutex mutex;
        std::condition_variable changed;
        std::size_t ready = 0;
        std::size_t done = 0;
        bool start = false;
        bool release = false;
        bool abort = false;
        bool failed = false;
    } gate;
    std::vector<WorkerResult> results(thread_count);
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    const auto cancel_and_join = [&] {
        {
            std::lock_guard lock(gate.mutex);
            gate.abort = true;
        }
        gate.changed.notify_all();
        for (auto& worker : workers) { worker.join(); }
    };
    try {
        for (std::size_t index = 0; index < thread_count; ++index) {
            workers.emplace_back([&, index] {
                bool announced = false;
                bool completed = false;
                try {
                    auto counters = factory.worker(index);
                    for (std::size_t iteration = 0; iteration < warm_iterations; ++iteration) {
                        (void)allocation_pair(allocator, counters, iteration, index);
                    }
                    bool should_run = false;
                    {
                        std::unique_lock lock(gate.mutex);
                        ++gate.ready;
                        announced = true;
                        gate.changed.notify_all();
                        gate.changed.wait(lock, [&] { return gate.start || gate.abort; });
                        should_run = !gate.abort;
                    }
                    // Only the fixed loop is timed. Launch, warmup, start gate,
                    // snapshots, validation and joining are all outside it.
                    if (should_run) {
                        std::uint64_t checksum = 0;
                        const auto begin = Clock::now();
                        for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
                            checksum += allocation_pair(allocator, counters, iteration, index);
                        }
                        const auto end = Clock::now();
                        results[index].seconds = std::chrono::duration<double>(end - begin).count();
                        results[index].started=begin;
                        results[index].finished=end;
                        results[index].checksum = checksum;
                        results[index].counters = counters.snapshot();
                    }
                    {
                        std::unique_lock lock(gate.mutex);
                        ++gate.done;
                        completed = true;
                        gate.changed.notify_all();
                        // Keep allocator thread teardown outside every worker's
                        // timed window, including after one worker finishes early.
                        gate.changed.wait(lock, [&] { return gate.release || gate.abort; });
                    }
                } catch (...) {
                    results[index].error = std::current_exception();
                    {
                        std::lock_guard lock(gate.mutex);
                        gate.failed = true;
                        if (!announced) { ++gate.ready; }
                        if (!completed) { ++gate.done; }
                    }
                    gate.changed.notify_all();
                }
            });
        }
        {
            std::unique_lock lock(gate.mutex);
            gate.changed.wait(lock, [&] { return gate.ready == thread_count; });
            gate.abort = gate.failed;
            gate.start = !gate.failed;
        }
        gate.changed.notify_all();
        {
            std::unique_lock lock(gate.mutex);
            gate.changed.wait(lock, [&] { return gate.done == thread_count; });
            gate.release = true;
        }
        gate.changed.notify_all();
    } catch (...) {
        // Workers warming or waiting also escape safely after launch failure.
        cancel_and_join();
        throw;
    }
    for (auto& worker : workers) { worker.join(); }
    Measurement result;
    for (const auto& worker : results) {
        if (worker.error) { std::rethrow_exception(worker.error); }
        require(worker.seconds>0,"statistics worker duration is zero");
        result.checksum += worker.checksum;
    }
    result.seconds=std::chrono::duration<double>(elapsed_span(results)).count();
    require(result.seconds > 0, "statistics diagnostic timer resolution too coarse");
    require(result.checksum == expected_checksum(iterations, thread_count),
            "statistics diagnostic content checksum mismatch");
    factory.validate(results, iterations + warm_iterations);
    return result;
}

template<class Native>
void run_backend(std::ostream& output, const std::string& backend_name,
                 unimem::Backend backend, const std::string& variant,
                 std::size_t iterations, std::size_t threads) {
    Native native;
    Measurement result;
    const std::string_view suite = variant.starts_with("synthetic_")
        ? "statistics_synthetic" : "statistics_actual";
    if (variant == "native" || variant == "synthetic_none") {
        NoCounters counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "api_disabled" || variant == "api_basic") {
        const bool basic = variant == "api_basic";
        unimem::Memory::configure_global(backend, basic ? unimem::StatisticsMode::Basic
                                                       : unimem::StatisticsMode::Disabled);
        auto& memory = unimem::Memory::global(backend);
        require(memory.capabilities().thread_safe,
                "statistics diagnostic requires shared thread-safe Global Memory");
        const auto before = memory.statistics();
        require(before.has_value() == basic,
                "statistics diagnostic actual statistics mode mismatch");
        if (basic) {
            require(before->live_bytes == 0,
                    "statistics diagnostic actual Memory starts with live allocations");
        }
        NoCounters counters;
        result = measure(ActualMemory{&memory}, counters, iterations, threads);
        if (basic) {
            const auto after = memory.statistics();
            const auto total = (static_cast<std::uint64_t>(iterations) + warm_iterations) * threads;
            require(after.has_value() && after->live_bytes == 0 &&
                        after->allocations - before->allocations == total &&
                        after->deallocations - before->deallocations == total &&
                        after->reallocations == before->reallocations,
                    "statistics diagnostic actual Basic totals mismatch");
        }
    } else if (variant == "synthetic_thread_local_plain") {
        ThreadLocalPlain counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_relaxed") {
        SharedAtomic<CompactCounters, CounterFeatures::Full> counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_padded") {
        SharedAtomic<PaddedCounters, CounterFeatures::Full> counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_worker_padded_atomic") {
        WorkerPaddedAtomic counters(threads);
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_worker_padded_plain") {
        WorkerPaddedPlain counters(threads);
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_no_peak") {
        SharedAtomic<CompactCounters, CounterFeatures::NoPeak> counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_peak_stress") {
        SharedAtomic<CompactCounters, CounterFeatures::StressFull> counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_peak_stress_no_peak") {
        SharedAtomic<CompactCounters, CounterFeatures::StressNoPeak> counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_live_only") {
        SharedAtomic<CompactCounters, CounterFeatures::LiveOnly> counters;
        result = measure(native, counters, iterations, threads);
    } else if (variant == "synthetic_shared_counts_only") {
        SharedAtomic<CompactCounters, CounterFeatures::CountsOnly> counters;
        result = measure(native, counters, iterations, threads);
    } else {
        throw std::invalid_argument("unknown statistics diagnostic variant: " + variant);
    }
    const auto operations = iterations * threads;
    output << suite << ',' << backend_name << ',' << variant << ',' << bytes << ','
           << threads << ',' << operations << ',' << std::setprecision(17)
           << result.seconds << ',' << result.seconds * 1e9 / operations
           << ",ns/pair," << result.checksum << '\n';
}

}

void run_statistics(std::ostream& output, const std::string& backend,
                    const std::string& variant, std::size_t iterations,
                    std::size_t threads) {
    verify_elapsed_span();
    if (iterations == 0 ||
        (threads != 1 && threads != 2 && threads != 4 && threads != 8 && threads != 16)) {
        throw std::invalid_argument("statistics diagnostic requires positive iterations and 1/2/4/8/16 threads");
    }
    if (iterations > std::numeric_limits<std::size_t>::max() - warm_iterations ||
        iterations + warm_iterations > std::numeric_limits<std::uint64_t>::max() / threads ||
        iterations > std::numeric_limits<std::size_t>::max() / threads) {
        throw std::invalid_argument("statistics diagnostic operation count overflow");
    }
    if (backend == "standard") {
        run_backend<NativeStandard>(output, backend, unimem::Backend::Standard,
                                    variant, iterations, threads);
        return;
    }
#ifdef UNIMEMORY_BENCH_MIMALLOC
    if (backend == "mimalloc") {
        run_backend<NativeMimalloc>(output, backend, unimem::Backend::Mimalloc,
                                   variant, iterations, threads);
        return;
    }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    if (backend == "jemalloc") {
        run_backend<NativeJemalloc>(output, backend, unimem::Backend::Jemalloc,
                                   variant, iterations, threads);
        return;
    }
#endif
    throw std::invalid_argument("statistics diagnostic backend is unavailable: " + backend);
}

}
