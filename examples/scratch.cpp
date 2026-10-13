#include <unimem/all.h>

#include <cstddef>
#include <iostream>

int main() {
  alignas(64) std::byte buffer[4096];
  unimem::Stack scratch(buffer, sizeof buffer);
  unimem::Stack::Mark checkpoint = scratch.mark();
  std::byte* bytes = static_cast<std::byte*>(scratch.allocate(128, 64));
  bytes[0] = std::byte{42};
  if (bytes[0] != std::byte{42}) {
    return 1;
  }
  scratch.rewind(checkpoint);
  std::cout << scratch.used() << " bytes used after rewind\n";
  return scratch.used() == 0 ? 0 : 2;
}
