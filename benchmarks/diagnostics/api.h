#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

namespace unimem_diagnostics {

// One backend/variant per fresh process. Emits one row using the caller's header:
// suite,backend,variant,bytes,threads,operations,seconds,value,unit,checksum
// Every operation is a complete allocation/use/destruction iteration.
void run_api(std::ostream& output, const std::string& backend,
             const std::string& variant, std::size_t iterations);

}
