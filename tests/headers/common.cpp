#include <unimem/common.h>
#include "common_api_contract.h"
bool header_common() {
    const auto cap = unimem::capabilities();
    unimem::MemoryStatistics statistics;
    unimem::BackendStatistics native;
    return cap.heap == (unimem::detail::build_backend != unimem::Backend::Standard) &&
           cap.release_delay == unimem::supports(unimem::RuntimeOption::UnusedPageReleaseDelayMs) &&
           statistics.live_bytes == 0 && !native.requested_bytes;
}
