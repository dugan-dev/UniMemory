#include <unimem/common.h>

bool header_common() {
    unimem::BackendCapabilities capabilities = unimem::capabilities(unimem::Backend::Standard);
    unimem::MemoryStatistics statistics;
    unimem::BackendStatistics backend_statistics;
    return unimem::available(unimem::Backend::Standard) && capabilities.available &&
           !capabilities.heap && statistics.live_bytes == 0 &&
           !backend_statistics.requested_bytes &&
           !unimem::supports(unimem::Backend::Standard,
                            unimem::RuntimeOption::UnusedPageReleaseDelayMs);
}
