#include "compiled-test-config.h"
#include <array>
#include <type_traits>
template<class T> concept HasRuntimeGlobalConfiguration = requires {
    T::configure_global(compiled_test::backend, compiled_test::global_mode);
};
static_assert(!HasRuntimeGlobalConfiguration<unimem::Memory>);
static_assert(!std::is_polymorphic_v<unimem::Memory>);
static_assert(std::is_same_v<decltype(unimem::Memory::global()), unimem::Memory&>);
int main() {
    using namespace unimem;
    compiled_test::verify_configuration();
    // Rejection precedes SDK construction, including selected Standard which
    // does not otherwise support Heap. Do not gate this on heap capability.
    if constexpr (!compiled_test::supports_statistics) {
        bool rejected = false;
        try { (void)Memory::heap(compiled_test::backend, StatisticsMode::Basic); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) { return 2; }
    }
    if (capabilities(compiled_test::backend).heap) {
        for (auto mode : compiled_test::heap_modes()) {
            auto heap = Memory::heap(compiled_test::backend, mode);
            auto item = heap.make_unique<int>(91);
            if (*item != 91 || !heap.owns(item.get()) ||
                heap.statistics().has_value() != (mode == StatisticsMode::Basic)) { return 1; }
            item.reset(); heap.reset();
        }
    }
    std::array<std::byte, 512> bytes{};
    auto stack = Memory::stack(bytes);
    auto object = stack.make_unique<int>(37);
    if (*object != 37 || stack.statistics() || stack.backend()) { return 3; }
    return 0;
}
