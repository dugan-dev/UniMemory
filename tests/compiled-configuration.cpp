#include "compiled-test-config.h"
#include <array>
#include <type_traits>
static_assert(!std::is_polymorphic_v<unimem::Memory>);
static_assert(std::is_same_v<decltype(unimem::Memory::global()), unimem::Memory&>);
int main() {
    using namespace unimem;
    compiled_test::verify_configuration();
    // Invalid-input cases belong in unit tests, not valid header consumers.
    const auto unknown_option = static_cast<RuntimeOption>(99);
    if (supports(unknown_option) || set_runtime_option(unknown_option, 0)) { return 6; }
    // Rejection precedes SDK construction, including selected Standard which
    // does not otherwise support Heap. Do not gate this on heap capability.
    if constexpr (!compiled_test::supports_statistics) {
        bool rejected = false;
        try { (void)Memory::heap(StatisticsMode::Basic); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) { return 2; }
    }
    if (capabilities().heap) {
        {
            auto heap = Memory::heap();
            auto item = heap.make_unique<int>(19);
            if (*item != 19 || !heap.owns(item.get()) || heap.statistics()) { return 4; }
        }
        for (auto mode : compiled_test::heap_modes()) {
            auto heap = Memory::heap(mode);
            auto item = heap.make_unique<int>(91);
            if (*item != 91 || !heap.owns(item.get()) ||
                heap.statistics().has_value() != (mode == StatisticsMode::Basic)) { return 1; }
            item.reset(); heap.reset();
        }
    }
    bool invalid_mode_rejected = false;
    try { (void)Memory::heap(static_cast<StatisticsMode>(99)); }
    catch (const std::invalid_argument&) { invalid_mode_rejected = true; }
    if (!invalid_mode_rejected) { return 5; }
    std::array<std::byte, 512> bytes{};
    auto stack = Memory::stack(bytes);
    auto object = stack.make_unique<int>(37);
    if (*object != 37 || stack.statistics() || stack.backend()) { return 3; }
    return 0;
}
