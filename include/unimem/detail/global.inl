#pragma once

namespace unimem {

inline Global::Global(StatisticsMode statistics) : Memory(MemoryKind::Global) {
  memory_impl::validate_statistics(statistics);
  initialize(memory_impl::selected_backend(), statistics);
}

}  // namespace unimem
