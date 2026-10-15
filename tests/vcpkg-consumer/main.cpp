#include <unimem/all.h>

#include <array>
#include <memory_resource>
#include <vector>

int main() {
  unimem::Global global;
  unimem::Heap heap;
  alignas(64) std::array<std::byte, 4096> buffer{};
  unimem::Stack stack(buffer.data(), buffer.size());
  for (unimem::Memory* memory : {static_cast<unimem::Memory*>(&global), static_cast<unimem::Memory*>(&heap), static_cast<unimem::Memory*>(&stack)}) {
    auto object = memory->make_unique<int>(42);
    std::pmr::vector<int> values(memory->resource());
    values.resize(16, 7);
    if (*object != 42 || values.back() != 7) {
      return 1;
    }
  }
}
