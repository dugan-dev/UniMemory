#include "../compiled-test-config.h"
#include "contract.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}
}  // namespace
int main() {
  try {
    compiled_test::verify_configuration();
    auto& memory = pmr_context();
    const auto a = pmr_identity_a(), b = pmr_identity_b();
    check(a.memory == &memory && b.memory == &memory && a.resource == b.resource && a.resource == memory.resource() &&
              a.resource->is_equal(*b.resource),
          "three-TU explicitly shared Global/PMR binding diverged");
    const auto before = memory.statistics();
    check(compiled_test::tracking() == (compiled_test::supports_statistics), "statistics profile mismatch");
    void* block = pmr_allocate_a(31, 16);
    void* zero = a.resource->allocate(0, 16);
    check(block && zero, "PMR zero normalization failed");
    const auto live = memory.statistics();
    if (compiled_test::tracking()) {
      check(compiled_test::tracking() && live.allocations == before.allocations + 2 && live.deallocations == before.deallocations &&
                live.reallocations == before.reallocations && live.live_bytes == before.live_bytes + 32 &&
                live.peak_live_bytes == std::max(before.peak_live_bytes, before.live_bytes + 32),
            "PMR exact live/peak ledger mismatch");
    }
    pmr_free_b(block, 31, 16);
    b.resource->deallocate(zero, 0, 16);
    memory.deallocate(nullptr, 0, 16);
    const auto after = memory.statistics();
    if (compiled_test::tracking()) {
      check(compiled_test::tracking() && after.allocations == before.allocations + 2 && after.deallocations == before.deallocations + 2 &&
                after.reallocations == before.reallocations && after.live_bytes == before.live_bytes && after.peak_live_bytes == live.peak_live_bytes,
            "cross-TU PMR zero/null/free ledger mismatch");
    }
    pmr_arm_late_call();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "PMR lifetime regression: " << error.what() << '\n';
    return 1;
  }
}
