#include <unimem/memory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory_resource>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace unimem;

void check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
template<class E, class F> void throws(F&& function) {
    try { function(); } catch (const E&) { return; }
    throw std::runtime_error("expected exception missing");
}
Backend parse_backend(const std::string& name) {
    if (name == "standard") { return Backend::Standard; }
    if (name == "mimalloc") { return Backend::Mimalloc; }
    if (name == "jemalloc") { return Backend::Jemalloc; }
    throw std::invalid_argument("unknown backend");
}

struct Context {
    std::vector<std::byte> buffer;
    std::unique_ptr<Memory> owned;
    Memory* memory = nullptr;
    Context(Backend backend, const std::string& kind, StatisticsMode mode) {
        if (kind == "global") { memory = &Memory::global(backend); }
        else if (kind == "heap") {
            owned.reset(new Memory(Memory::heap(backend, mode)));
            memory = owned.get();
        } else if (kind == "stack") {
            buffer.resize(4 * 1024 * 1024);
            owned.reset(new Memory(Memory::stack(buffer)));
            memory = owned.get();
        } else { throw std::invalid_argument("unknown memory kind"); }
    }
};

void balanced(Memory& memory, std::uint64_t baseline = 0) {
    if (auto stats = memory.statistics()) {
        check(stats->live_bytes == baseline, "requested bytes did not balance");
    }
}
void verify_bytes(const void* pointer, std::size_t bytes, unsigned char value) {
    if (bytes == 0) { return; }
    check(pointer != nullptr, "missing byte storage");
    const auto* data = static_cast<const unsigned char*>(pointer);
    check(std::all_of(data, data + bytes,
        [value](unsigned char byte) { return byte == value; }), "byte content mismatch");
}

void owners(Memory& first, Memory& second) {
    {
        auto a = first.make_block(31, 64);
        auto b = second.make_block(47, 256);
        std::memset(a.data(), 17, a.size());
        std::memset(b.data(), 23, b.size());
        auto* original = a.data();
        b = std::move(a);
        check(!a.data() && a.size() == 0 && b.data() == original &&
              b.size() == 31 && b.alignment() == 64, "cross Memory block move");
        balanced(second);
        b.resize(62);
        verify_bytes(b.data(), 31, 17);
        if (auto stats = first.statistics()) {
            check(stats->live_bytes == 62, "moved block changed Memory binding");
        }
        auto& self = b;
        b = std::move(self);
        verify_bytes(b.data(), 31, 17);
        throws<std::logic_error>([&] { a.resize(1); });
        a = first.make_block(0, 256);
        a.resize(19);
        check(a.size() == 19 && a.alignment() == 256, "empty block growth");
        std::memset(a.data(), 29, 19);
        a.resize(0);
        check(!a.data() && a.size() == 0, "block release to empty");
        a.resize(11);
        check(a.data() != nullptr && a.alignment() == 256, "released block regrowth");
    }
    balanced(first);
    balanced(second);
    {
        auto a = first.make_block(37, 64);
        auto b = second.make_block(53, 256);
        std::memset(a.data(), 41, 37);
        std::memset(b.data(), 43, 53);
        std::swap(a, b);
        check(a.size() == 53 && a.alignment() == 256 && b.size() == 37 &&
              b.alignment() == 64, "cross Memory block swap metadata");
        verify_bytes(a.data(), 53, 43);
        verify_bytes(b.data(), 37, 41);
        a.resize(0);
        balanced(second);
        if (auto stats = first.statistics()) {
            check(stats->live_bytes == 37, "block swap lost original Memory");
        }
    }
    balanced(first);
    // Bounded, deterministic exhaustion proves resize's strong failure guarantee.
    alignas(256) std::array<std::byte, 256> backing{};
    Memory limited = Memory::stack(backing);
    auto block = limited.make_block(32, 256);
    std::memset(block.data(), 47, 32);
    auto* original = block.data();
    const auto used = limited.used();
    throws<std::bad_alloc>([&] { block.resize(257); });
    check(block.data() == original && block.size() == 32 &&
          block.alignment() == 256 && limited.used() == used, "failed block resize changed state");
    verify_bytes(block.data(), 32, 47);
}

struct Record {
    static inline std::atomic<unsigned> alive{0}, made{0}, destroyed{0};
    unsigned value = 0;
    std::array<unsigned, 7> payload{};
    explicit Record(unsigned initial = 0) noexcept : value(initial) {
        payload.fill(initial);
        alive.fetch_add(1, std::memory_order_relaxed);
        made.fetch_add(1, std::memory_order_relaxed);
    }
    ~Record() noexcept {
        alive.fetch_sub(1, std::memory_order_relaxed);
        destroyed.fetch_add(1, std::memory_order_relaxed);
    }
    void set(unsigned next) noexcept { value = next; payload.fill(next); }
};
void verify_record(const Record& object, unsigned value) {
    check(object.value == value && std::all_of(object.payload.begin(), object.payload.end(),
        [value](unsigned word) { return word == value; }), "object content mismatch");
}
void record_balance() {
    check(Record::alive.load() == 0 && Record::made.load() == Record::destroyed.load(),
          "object lifetimes did not balance");
}

struct Construction {
    static inline unsigned attempts = 0, alive = 0, fail_at = 0, destruction_count = 0;
    static inline std::array<unsigned, 16> order{};
    unsigned id;
    Construction() : id(++attempts) {
        if (id == fail_at) { throw std::runtime_error("construction failure"); }
        ++alive;
    }
    ~Construction() noexcept { order[destruction_count++] = id; --alive; }
    static void start(unsigned fail) {
        check(alive == 0, "previous construction case leaked");
        attempts = destruction_count = 0;
        fail_at = fail;
        order.fill(0);
    }
};
struct Base {
    static inline unsigned destroyed = 0;
    virtual unsigned read() const noexcept = 0;
    virtual ~Base() noexcept { ++destroyed; }
};
struct alignas(256) Derived final : Base {
    static inline unsigned destroyed = 0;
    unsigned value;
    std::array<std::byte, 257> extra{};
    explicit Derived(unsigned initial) noexcept : value(initial) {}
    unsigned read() const noexcept override { return value; }
    ~Derived() noexcept override { ++destroyed; }
};
struct MoveArgument {
    std::unique_ptr<unsigned> value;
    explicit MoveArgument(std::unique_ptr<unsigned> input) noexcept : value(std::move(input)) {}
};

void objects(Memory& first, Memory& second) {
    {
        Unique<Record> empty_unique;
        UniqueArray<Record> empty_array;
        check(!empty_unique && !empty_array, "default empty owner acquired storage");
        Deleter<Record>{}(nullptr);
        ArrayDeleter<Record>{}(nullptr);
    }
    for (unsigned factory = 0; factory < 3; ++factory) {
        Construction::start(1);
        throws<std::runtime_error>([&] {
            if (factory == 0) { (void)first.create<Construction>(); }
            else if (factory == 1) { auto p = first.make_unique<Construction>(); }
            else { auto p = first.make_shared<Construction>(); }
        });
        check(Construction::alive == 0 && Construction::destruction_count == 0,
              "failed scalar constructor acquired a lifetime");
        balanced(first);
    }
    for (bool unique : {false, true}) {
        Construction::start(4);
        throws<std::runtime_error>([&] {
            if (unique) { auto p = first.make_unique_array<Construction>(6); }
            else { (void)first.create_array<Construction>(6); }
        });
        check(Construction::alive == 0 && Construction::destruction_count == 3 &&
              Construction::order[0] == 3 && Construction::order[1] == 2 &&
              Construction::order[2] == 1, "array rollback order");
        balanced(first);
    }
    Construction::start(0);
    auto* array = first.create_array<Construction>(3);
    first.destroy_array(array, 3);
    check(Construction::order[0] == 3 && Construction::order[1] == 2 &&
          Construction::order[2] == 1, "array destruction order");
    check(first.create_array<Construction>(0) == nullptr, "raw empty array");
    auto empty = first.make_unique_array<Construction>(0);
    check(!empty, "unique empty array");
    Deleter<Construction>{&first}(nullptr);
    ArrayDeleter<Construction>{&first, 3}(nullptr);
    first.destroy<Construction>(nullptr);
    first.destroy_array<Construction>(nullptr, 0);
    auto argument = std::make_unique<unsigned>(71);
    auto* forwarded = first.create<MoveArgument>(std::move(argument));
    check(!argument && *forwarded->value == 71, "constructor argument forwarding");
    first.destroy(forwarded);
    {
        auto a = first.make_unique<Record>(73);
        auto b = second.make_unique<Record>(79);
        b = std::move(a);
        check(!a && b.get_deleter().memory == &first, "unique move lost deleter binding");
        balanced(second);
        auto replacement = first.make_unique<Record>(83);
        b.reset(replacement.release());
        verify_record(*b, 83);
        auto other = second.make_unique<Record>(89);
        b.swap(other);
        check(b.get_deleter().memory == &second && other.get_deleter().memory == &first,
              "unique swap lost deleter binding");
        verify_record(*b, 89);
        auto* released = other.release();
        check(!other, "unique release retained ownership");
        auto adopted = first.adopt_unique(released);
        verify_record(*adopted, 83);
        auto* readopted = adopted.release();
        adopted = first.adopt_unique(readopted);
        verify_record(*adopted, 83);
    }
    {
        auto a = first.make_unique_array<Record>(2);
        auto b = second.make_unique_array<Record>(5);
        a[1].set(97);
        b[4].set(101);
        a.swap(b);
        check(a.get_deleter().memory == &second && a.get_deleter().count == 5 &&
              b.get_deleter().memory == &first && b.get_deleter().count == 2,
              "array swap lost Memory or count");
        verify_record(a[4], 101);
        verify_record(b[1], 97);
        auto* raw = b.release();
        auto adopted = first.adopt_unique_array(raw, 2);
        verify_record(adopted[1], 97);
        auto* replacement = a.release();
        adopted = second.adopt_unique_array(replacement, 5);
        check(adopted.get_deleter().memory == &second && adopted.get_deleter().count == 5,
              "adopted array replacement retained previous count or Memory");
        verify_record(adopted[4], 101);
    }
    {
        auto empty_unique = first.adopt_unique<Record>(nullptr);
        auto empty_array = first.adopt_unique_array<Record>(nullptr, 0);
        check(!empty_unique && !empty_array, "null adoption acquired ownership");
    }
    {
        auto shared = first.make_shared<Record>(103);
        std::weak_ptr<Record> weak = shared;
        auto copy = shared;
        auto locked = weak.lock();
        check(locked && shared.use_count() == 3, "weak lock acquired wrong ownership");
        std::shared_ptr<unsigned> alias(shared, &shared->value);
        shared.reset(); copy.reset(); locked.reset();
        check(!weak.expired() && *alias == 103, "alias lost object ownership");
        alias.reset();
        check(weak.expired() && !weak.lock(), "last strong release retained object");
        if (auto stats = first.statistics()) {
            check(stats->live_bytes > 0, "weak pointer lost its control block");
        }
        weak.reset();
    }
#if defined(__cpp_lib_shared_ptr_arrays) && __cpp_lib_shared_ptr_arrays >= 201707L
    {
        auto shared_array = first.make_shared<Record[]>(3);
        std::weak_ptr<Record[]> weak = shared_array;
        for (unsigned index = 0; index < 3; ++index) {
            verify_record(shared_array[index], 0);
            shared_array[index].set(151 + index);
        }
        auto locked = weak.lock();
        shared_array.reset();
        verify_record(locked[2], 153);
        locked.reset();
        check(weak.expired() && !weak.lock(), "shared array weak lifetime");
        weak.reset();
    }
    Construction::start(4);
    throws<std::runtime_error>([&] { auto shared_array = first.make_shared<Construction[]>(6); });
    check(Construction::alive == 0 && Construction::destruction_count == 3,
          "shared array rollback lifetime balance");
    const bool reverse = Construction::order[0] == 3 && Construction::order[1] == 2 &&
                         Construction::order[2] == 1;
#if defined(__GLIBCXX__) && defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 13 && _GLIBCXX_RELEASE <= 14
    // Check the known libstdc++ exception-cleanup limitation against a fresh
    // standalone STL call. Never accept a UniMemory-only rollback difference.
    const bool reproduced_forward = Construction::order[0] == 1 && Construction::order[1] == 2 &&
                                    Construction::order[2] == 3;
    check(reverse || reproduced_forward, "shared array rollback order outside known compatibility");
    if (reproduced_forward) {
        Construction::start(4);
        throws<std::runtime_error>([] { auto p = std::allocate_shared<Construction[]>(std::allocator<Construction>(), 6); });
        check(Construction::alive == 0 && Construction::destruction_count == 3 &&
              Construction::order[0] == 1 && Construction::order[1] == 2 && Construction::order[2] == 3,
              "UniMemory differs from standalone libstdc++ rollback");
        std::cerr << "libstdc++ " << _GLIBCXX_RELEASE << " compatibility: shared array constructor rollback destroys "
                     "elements in forward order (1,2,3); C++20 requires reverse order (3,2,1).\n";
    }
#else
    check(reverse, "shared array rollback order");
#endif
#endif
    const auto before_base = Base::destroyed;
    const auto before_derived = Derived::destroyed;
    auto* raw = first.create<Derived>(107);
    Base* observer = raw;
    check(observer->read() == 107, "raw polymorphic observer");
    first.destroy(raw); // Keep the exact allocation type for storage release.
    {
        auto unique = first.make_unique<Derived>(109);
        observer = unique.get();
        check(observer->read() == 109, "unique polymorphic observer");
        auto* released = unique.release();
        first.destroy(released);
    }
    {
        std::shared_ptr<Base> shared = first.make_shared<Derived>(113);
        std::weak_ptr<Base> weak = shared;
        check(shared->read() == 113, "shared polymorphic conversion");
        shared.reset();
        check(weak.expired(), "polymorphic weak lifetime");
        weak.reset();
    }
    check(Base::destroyed - before_base == 3 && Derived::destroyed - before_derived == 3,
          "polymorphic destruction count");
    record_balance();
    balanced(first); balanced(second);
}

void allocators(Memory& first, Memory& second) {
    using A = Allocator<unsigned>;
    using Traits = std::allocator_traits<A>;
    static_assert(!Traits::propagate_on_container_copy_assignment::value);
    static_assert(!Traits::propagate_on_container_move_assignment::value);
    static_assert(!Traits::propagate_on_container_swap::value);
    static_assert(!Traits::is_always_equal::value);
    auto a = first.allocator<unsigned>();
    auto copied = a;
    auto moved = std::move(copied);
    typename Traits::template rebind_alloc<Record> rebound(a);
    check(a == moved && a == rebound && !(a != rebound), "allocator copy/rebind equality");
    auto b = second.allocator<unsigned>();
    check(a != b, "distinct Memory allocators compare equal");
    copied = b;
    moved = std::move(copied);
    check(moved == b, "allocator assignment binding");
    std::swap(a, moved);
    check(a == b && &moved.memory() == &first, "allocator swap binding");
    auto* zero = a.allocate(0);
    check(zero != nullptr, "allocator zero-count storage");
    a.deallocate(zero, 0);
    auto* records = rebound.allocate(2);
    rebound.deallocate(records, 2);
    throws<std::length_error>([&] {
        rebound.allocate(std::numeric_limits<std::size_t>::max());
    });
    {
        std::vector<unsigned, A> source(first.allocator<unsigned>());
        source.assign({1, 2, 3, 5, 8});
        auto copy = source;
        check(copy == source && &copy.get_allocator().memory() == &first,
              "container copy construction binding");
        auto* original = source.data();
        std::vector<unsigned, A> moved_vector(std::move(source));
        check(moved_vector.data() == original && moved_vector.back() == 8,
              "equal allocator move construction");
        source.assign({13, 21});
        moved_vector.swap(source); // Nonpropagating allocator swap requires equality.
        check(moved_vector.back() == 21 && source.back() == 8, "equal allocator container swap");
        std::vector<unsigned, A> target(second.allocator<unsigned>());
        target = source;
        check(target == source && &target.get_allocator().memory() == &second,
              "container copy assignment propagation");
        target = std::move(source);
        check(target.back() == 8 && &target.get_allocator().memory() == &second,
              "unequal allocator move assignment propagation");
        source.assign({34, 55});
        std::vector<unsigned, A> explicit_move(std::move(source), second.allocator<unsigned>());
        check(explicit_move.back() == 55 && &explicit_move.get_allocator().memory() == &second,
              "explicit allocator move construction");
        std::list<unsigned, A> nodes(first.allocator<unsigned>());
        nodes.assign({3, 7, 11});
        using Pair = std::pair<const unsigned, unsigned>;
        std::map<unsigned, unsigned, std::less<unsigned>, Allocator<Pair>> map(
            std::less<unsigned>{}, first.allocator<Pair>());
        map.emplace(17, nodes.back());
        check(map.at(17) == 11, "node allocator rebind");
    }
    {
        auto* resource = first.resource();
        check(resource == first.resource() && resource->is_equal(*resource) &&
              !resource->is_equal(*second.resource()), "PMR resource identity");
        auto* zero_storage = resource->allocate(0, 256);
        check(zero_storage != nullptr && reinterpret_cast<std::uintptr_t>(zero_storage) % 256 == 0,
              "PMR zero storage alignment");
        resource->deallocate(zero_storage, 0, 256);
        std::pmr::vector<std::pmr::string> nested(resource);
        nested.emplace_back(128, 'x');
        auto copy = nested;
        check(copy.get_allocator().resource() == std::pmr::get_default_resource() &&
              copy.front().get_allocator().resource() == std::pmr::get_default_resource(),
              "PMR copy selection");
        std::pmr::vector<std::pmr::string> target(nested, second.resource());
        check(target.front().get_allocator().resource() == second.resource() && target == nested,
              "nested PMR explicit copy binding");
        target = std::move(nested);
        check(target.front().get_allocator().resource() == second.resource() &&
              target.front().size() == 128 && std::all_of(target.front().begin(), target.front().end(),
              [](char byte) { return byte == 'x'; }), "nested PMR move assignment binding");
        std::pmr::vector<unsigned> left(resource), right(resource);
        left.assign({1, 2}); right.assign({3, 4, 5});
        left.swap(right);
        check(left.back() == 5 && right.back() == 2, "equal resource PMR swap");
    }
    balanced(first); balanced(second);
}

void marks(Memory& stack) {
    check(stack.kind() == MemoryKind::Stack, "marks mode requires Stack");
    stack.reset();
    auto outer = stack.mark();
    auto outer_copy = outer;
    auto* first = stack.allocate(17, 1);
    std::memset(first, 127, 17);
    auto middle = stack.mark();
    auto* second = stack.allocate(23, 1);
    std::memset(second, 131, 23);
    auto inner = stack.mark();
    auto assigned = outer;
    assigned = inner;
    stack.allocate(29, 1);
    stack.rewind(assigned);
    check(stack.used() == 40, "copied inner mark rewind");
    verify_bytes(first, 17, 127); verify_bytes(second, 23, 131);
    for (auto old : {outer, outer_copy, middle, inner, assigned}) {
        const auto used = stack.used();
        throws<std::invalid_argument>([&] { stack.rewind(old); });
        check(stack.used() == used, "stale mark rejection changed offset");
        verify_bytes(first, 17, 127); verify_bytes(second, 23, 131);
    }
    auto fresh = stack.mark();
    stack.allocate(13, 1);
    stack.rewind(fresh);
    check(stack.used() == 40, "fresh mark after rewind");
    auto before_reset = stack.mark();
    stack.reset();
    throws<std::invalid_argument>([&] { stack.rewind(before_reset); });
    check(stack.used() == 0, "reset mark rejection changed offset");
}

// Start only after all threads exist, so a thread-creation failure cannot strand a barrier.
template<class F> void parallel(unsigned count, F&& function) {
    std::atomic<bool> start{false}, abort{false};
    std::vector<std::thread> workers;
    workers.reserve(count);
    try {
        for (unsigned id = 0; id < count; ++id) {
            workers.emplace_back([&, id] {
                start.wait(false, std::memory_order_acquire);
                if (!abort.load(std::memory_order_relaxed)) { function(id); }
            });
        }
    } catch (...) {
        abort.store(true, std::memory_order_relaxed);
        start.store(true, std::memory_order_release); start.notify_all();
        for (auto& worker : workers) { worker.join(); }
        throw;
    }
    start.store(true, std::memory_order_release); start.notify_all();
    for (auto& worker : workers) { worker.join(); }
}

void global_config(Backend backend, unsigned scenario, StatisticsMode mode) {
    constexpr unsigned count = 8;
    std::atomic<bool> failed{false};
    std::array<Memory*, count> addresses{};
    if (scenario == 0) {
        parallel(count, [&](unsigned) {
            try { Memory::configure_global(backend, mode); }
            catch (...) { failed.store(true); }
        });
        check(!failed.load(), "concurrent identical preconfiguration");
    } else if (scenario == 1 || scenario == 2) {
        Memory::configure_global(backend, mode);
        (void)Memory::global(backend);
    } else { throw std::invalid_argument("global-config seed must be 0, 1, or 2"); }
    parallel(count, [&](unsigned id) {
        try {
            for (unsigned round = 0; round < 64; ++round) {
                if (scenario == 2) {
                    throws<std::logic_error>([&] {
                        Memory::configure_global(backend, mode == StatisticsMode::Basic
                            ? StatisticsMode::Disabled : StatisticsMode::Basic);
                    });
                } else { Memory::configure_global(backend, mode); }
                addresses[id] = &Memory::global(backend);
                check(addresses[id]->statistics().has_value() == (mode == StatisticsMode::Basic),
                      "concurrent configuration changed statistics mode");
            }
        } catch (...) { failed.store(true); }
    });
    check(!failed.load(), "concurrent Global configuration or lookup");
    for (auto* address : addresses) {
        check(address == addresses[0] && address != nullptr, "Global identity diverged");
    }
}

struct Transfer {
    Record* raw = nullptr;
    std::optional<OwnedBlock> block;
    std::optional<Unique<Record>> unique;
    std::optional<UniqueArray<Record>> array;
    std::shared_ptr<Record> shared;
    std::weak_ptr<Record> weak;
    unsigned value = 0, acknowledgement = 0;
    void clear(Memory& memory) noexcept {
        memory.destroy(raw); raw = nullptr;
        block.reset(); unique.reset(); array.reset(); shared.reset(); weak.reset();
    }
};

void handoff(Memory& memory) {
    check(memory.kind() != MemoryKind::Stack, "handoff requires Global or Heap");
    constexpr unsigned count = 4, batch = 8, rounds = 64;
    std::array<std::array<Transfer, batch>, count> slots;
    std::barrier phase(count);
    std::atomic<bool> failed{false};
    auto anchor = memory.make_block(193, 256);
    std::memset(anchor.data(), 139, anchor.size());
    const auto baseline = memory.statistics();
    parallel(count, [&](unsigned id) {
        for (unsigned round = 0; round < rounds; ++round) {
            for (unsigned item = 0; item < batch; ++item) {
                auto& slot = slots[id][item];
                try {
                    slot.value = 1 + round * count * batch + id * batch + item;
                    slot.acknowledgement = 0;
                    slot.raw = memory.create<Record>(slot.value);
                    slot.block.emplace(memory.make_block(96, 64));
                    std::memset(slot.block->data(), slot.value % 251, 96);
                    slot.unique.emplace(memory.make_unique<Record>(slot.value));
                    slot.array.emplace(memory.make_unique_array<Record>(3));
                    for (unsigned index = 0; index < 3; ++index) {
                        (*slot.array)[index].set(slot.value + index);
                    }
                    slot.shared = memory.make_shared<Record>(slot.value);
                    slot.weak = slot.shared;
                } catch (...) { failed.store(true); }
            }
            phase.arrive_and_wait(); // Publish every pointer and initial object write.
            const auto producer = (id + count - 1) % count;
            for (auto& slot : slots[producer]) {
                try {
                    if (slot.raw) {
                        verify_record(*slot.raw, slot.value);
                        slot.raw->set(slot.value + 1);
                        slot.acknowledgement = slot.raw->value;
                        memory.destroy(slot.raw); slot.raw = nullptr;
                    }
                    if (slot.block) {
                        auto block = std::move(*slot.block);
                        verify_bytes(block.data(), 96, static_cast<unsigned char>(slot.value % 251));
                        block.resize(144);
                        verify_bytes(block.data(), 96, static_cast<unsigned char>(slot.value % 251));
                        std::memset(block.data(), (slot.value + 1) % 251, 144);
                        slot.block.emplace(std::move(block));
                    }
                    if (slot.unique) {
                        auto owner = std::move(*slot.unique);
                        verify_record(*owner, slot.value); owner->set(slot.value + 1);
                        slot.unique.reset(); // Local owner dies on the consumer thread.
                    }
                    if (slot.array) {
                        auto owner = std::move(*slot.array);
                        for (unsigned index = 0; index < 3; ++index) {
                            verify_record(owner[index], slot.value + index);
                            owner[index].set(slot.value + index + 1);
                        }
                        slot.array.reset();
                    }
                    if (slot.shared) {
                        auto local = slot.weak.lock();
                        check(local != nullptr, "published weak lock failed");
                        slot.shared.reset();
                        verify_record(*local, slot.value); local->set(slot.value + 1);
                        local.reset();
                        check(slot.weak.expired(), "consumer last shared release");
                    }
                } catch (...) { failed.store(true); }
            }
            phase.arrive_and_wait(); // Publish consumer writes before producer reads/reuses slots.
            for (auto& slot : slots[id]) {
                try {
                    check(slot.acknowledgement == slot.value + 1, "object handoff write visibility");
                    if (slot.block) {
                        check(slot.block->size() == 144, "remote block resize");
                        verify_bytes(slot.block->data(), 144,
                                     static_cast<unsigned char>((slot.value + 1) % 251));
                    }
                    check(slot.weak.expired(), "handoff weak retained object");
                } catch (...) { failed.store(true); }
                slot.clear(memory);
            }
        }
    });
    // No worker can overlap these control operations, even on allocation failure.
    for (auto& lane : slots) { for (auto& slot : lane) { slot.clear(memory); } }
    check(!failed.load(), "producer-consumer allocation, ownership, or content failure");
    record_balance();
    verify_bytes(anchor.data(), anchor.size(), 139);
    if (memory.kind() == MemoryKind::Heap) {
        const auto before = memory.statistics();
        memory.collect();
        verify_bytes(anchor.data(), anchor.size(), 139);
        check(memory.owns(anchor.data()), "collect lost live anchor ownership");
        if (before) {
            const auto after = *memory.statistics();
            check(before->allocations == after.allocations && before->deallocations == after.deallocations &&
                  before->reallocations == after.reallocations && before->live_bytes == after.live_bytes &&
                  before->peak_live_bytes == after.peak_live_bytes, "collect changed anchor accounting");
        }
    }
    if (baseline) {
        const auto after = *memory.statistics();
        check(after.live_bytes == baseline->live_bytes &&
              after.allocations - baseline->allocations == after.deallocations - baseline->deallocations,
              "handoff allocation counts did not balance");
    }
    anchor.resize(0);
    balanced(memory);
    if (memory.kind() == MemoryKind::Heap) { memory.reset(); }
}

using Payload = std::variant<std::monostate, OwnedBlock, Unique<Record>,
                             UniqueArray<Record>, std::shared_ptr<Record>>;
struct Slot {
    Payload payload;
    std::weak_ptr<Record> weak;
    unsigned value = 0;
    std::size_t count = 0;
    unsigned char pattern = 0;
    void clear() noexcept { payload.emplace<0>(); weak.reset(); count = 0; }
};
void verify(const Slot& slot) {
    if (const auto* block = std::get_if<OwnedBlock>(&slot.payload)) {
        check(block->size() == slot.count && (!block->data() ||
              reinterpret_cast<std::uintptr_t>(block->data()) % block->alignment() == 0),
              "trace block metadata");
        verify_bytes(block->data(), slot.count, slot.pattern);
    } else if (const auto* unique = std::get_if<Unique<Record>>(&slot.payload)) {
        check(bool(*unique), "trace unique lost ownership"); verify_record(**unique, slot.value);
    } else if (const auto* array = std::get_if<UniqueArray<Record>>(&slot.payload)) {
        check(bool(*array), "trace array lost ownership");
        for (std::size_t index = 0; index < slot.count; ++index) {
            verify_record((*array)[index], slot.value + static_cast<unsigned>(index));
        }
    } else if (const auto* shared = std::get_if<std::shared_ptr<Record>>(&slot.payload)) {
        if (*shared) {
            verify_record(**shared, slot.value);
            auto locked = slot.weak.lock();
            check(locked == *shared, "trace weak lock identity");
        } else { check(slot.weak.expired() && !slot.weak.lock(), "trace expired weak lifetime"); }
    }
}
void trace(Memory& memory, unsigned seed, unsigned steps) {
    std::mt19937 random(seed);
    std::array<Slot, 16> slots;
    const auto baseline = memory.statistics();
    for (unsigned step = 0; step < steps; ++step) {
        for (const auto& slot : slots) { verify(slot); }
        auto& slot = slots[random() % slots.size()];
        if (slot.payload.index() == 0) {
            slot.value = random();
            slot.pattern = static_cast<unsigned char>(random());
            switch (random() % 4) {
            case 0: {
                slot.count = random() % 257;
                slot.payload.emplace<1>(memory.make_block(slot.count, std::size_t{1} << (random() % 9)));
                auto& block = std::get<1>(slot.payload);
                if (slot.count) { std::memset(block.data(), slot.pattern, slot.count); }
                break;
            }
            case 1: slot.payload.emplace<2>(memory.make_unique<Record>(slot.value)); break;
            case 2: {
                slot.count = 1 + random() % 8;
                slot.payload.emplace<3>(memory.make_unique_array<Record>(slot.count));
                auto& array = std::get<3>(slot.payload);
                for (std::size_t index = 0; index < slot.count; ++index) {
                    array[index].set(slot.value + static_cast<unsigned>(index));
                }
                break;
            }
            default:
                slot.payload.emplace<4>(memory.make_shared<Record>(slot.value));
                slot.weak = std::get<4>(slot.payload);
            }
        } else {
            switch (random() % 5) {
            case 0: slot.clear(); break;
            case 1: {
                auto& destination = slots[random() % slots.size()];
                if (&destination != &slot) {
                    destination = std::move(slot);
                    slot.clear();
                }
                break;
            }
            case 2: {
                auto& other = slots[random() % slots.size()];
                if (&other != &slot) { std::swap(other, slot); }
                break;
            }
            case 3:
            case 4:
                if (auto* block = std::get_if<OwnedBlock>(&slot.payload)) {
                    const auto size = std::size_t{random() % 513};
                    block->resize(size);
                    verify_bytes(block->data(), std::min(slot.count, size), slot.pattern);
                    slot.count = size;
                    if (size) { std::memset(block->data(), slot.pattern, size); }
                } else if (auto* shared = std::get_if<std::shared_ptr<Record>>(&slot.payload)) {
                    if (random() % 2) {
                        auto copy = *shared;
                        auto locked = slot.weak.lock();
                        check(copy == locked, "trace shared copy/weak lock");
                    } else { shared->reset(); }
                } else if (auto* unique = std::get_if<Unique<Record>>(&slot.payload)) {
                    auto* raw = unique->release();
                    verify_record(*raw, slot.value);
                    memory.destroy(raw);
                    slot.clear();
                } else {
                    auto& array = std::get<UniqueArray<Record>>(slot.payload);
                    const auto count = array.get_deleter().count;
                    auto* raw = array.release();
                    memory.destroy_array(raw, count);
                    slot.clear();
                }
                break;
            }
        }
    }
    for (auto& slot : slots) { verify(slot); slot.clear(); }
    record_balance();
    balanced(memory);
    if (baseline) {
        const auto after = *memory.statistics();
        check(after.allocations - baseline->allocations == after.deallocations - baseline->deallocations,
              "owner trace allocation counts did not balance");
    }
    if (memory.kind() == MemoryKind::Stack) {
        check(memory.used() > 0 || steps == 0, "Stack trace unexpectedly reclaimed individual storage");
        memory.reset();
        check(memory.used() == 0, "Stack trace epoch reset");
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 4 || argc > 7) {
            throw std::invalid_argument("usage: backend kind mode [seed] [steps] [basic|disabled]");
        }
        const auto backend = parse_backend(argv[1]);
        check(available(backend), "backend unavailable");
        const std::string kind = argv[2], mode = argv[3];
        const auto seed = argc >= 5 ? static_cast<unsigned>(std::stoul(argv[4])) : 0;
        const auto steps = argc >= 6 ? static_cast<unsigned>(std::stoul(argv[5])) : 1024;
        const std::string statistics = argc >= 7 ? argv[6] : "basic";
        if (statistics != "basic" && statistics != "disabled") {
            throw std::invalid_argument("statistics must be basic or disabled");
        }
        const auto tracking = statistics == "basic" ? StatisticsMode::Basic : StatisticsMode::Disabled;
        if (mode == "global-config") {
            check(kind == "global", "global-config requires Global");
            global_config(backend, seed, tracking);
            return 0;
        }
        if (kind == "global") { Memory::configure_global(backend, tracking); }
        Context context(backend, kind, tracking);
        auto& memory = *context.memory;
        if (mode == "trace") { trace(memory, seed, steps); }
        else if (mode == "marks") { marks(memory); }
        else if (mode == "handoff") { handoff(memory); }
        else {
            Context peer(backend, kind == "heap" ? "heap" : "stack", tracking);
            if (mode == "owners") { owners(memory, *peer.memory); }
            else if (mode == "objects") { objects(memory, *peer.memory); }
            else if (mode == "allocators") { allocators(memory, *peer.memory); }
            else { throw std::invalid_argument("unknown usage mode"); }
            balanced(*peer.memory);
            if (peer.memory->kind() == MemoryKind::Stack) { peer.memory->reset(); }
        }
        balanced(memory);
        if (memory.kind() == MemoryKind::Stack) { memory.reset(); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "interface usage";
        for (int index = 1; index < argc; ++index) { std::cerr << ' ' << argv[index]; }
        std::cerr << ": " << error.what() << '\n';
        return 1;
    }
}
