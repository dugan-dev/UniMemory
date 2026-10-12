#include <unimem/common.h>
#include "types_api_contract.h"
bool header_types() {
  const unimem::Statistics statistics;
  return statistics.allocations == 0 && statistics.deallocations == 0 && statistics.reallocations == 0 && statistics.live_bytes == 0 &&
         statistics.peak_live_bytes == 0;
}
