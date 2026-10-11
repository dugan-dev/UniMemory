#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

namespace unimem_diagnostics {

// One selection per fresh process: Memory::global cannot change statistics mode.
// Emits one CSV row; the caller supplies the common CSV header.
void run_statistics(std::ostream& output, const std::string& backend,
                    const std::string& variant, std::size_t iterations,
                    std::size_t threads);

}
