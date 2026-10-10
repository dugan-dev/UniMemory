#include <unimem/common.h>
#include <initializer_list>
bool header_common() {
    unsigned enabled=0;
    for (auto backend : {unimem::Backend::Standard,unimem::Backend::Mimalloc,unimem::Backend::Jemalloc}) {
        const auto cap=unimem::capabilities(backend);
        if (cap.available != unimem::available(backend)) { return false; }
        enabled += cap.available;
    }
    unimem::MemoryStatistics statistics;
    unimem::BackendStatistics native;
    return enabled==1 && statistics.live_bytes==0 && !native.requested_bytes;
}
