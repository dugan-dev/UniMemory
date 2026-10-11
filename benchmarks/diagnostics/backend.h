#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

namespace unimem_diagnostics {

// CSV data rows only; the caller writes the shared header and starts a fresh
// process for each backend/variant. Unsupported combinations throw.
void run_backend(std::ostream& out, const std::string& variant,
                 const std::string& backend, std::size_t iterations);

}
