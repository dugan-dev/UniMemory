#pragma once
#include <unimem/memory.h>
#include "common_api_contract.h"

namespace test_api {
template<class T> concept HasSelectedBackend = requires { T::selected_backend; };
template<class T> concept HasSelectedStatistics = requires { T::selected_statistics; };
template<class T> concept HasRuntimeGlobalConfiguration = requires {
    T::configure_global(unimem::Backend::Standard, unimem::StatisticsMode::Disabled);
};
template<class T> concept HasBackendGlobal = requires(T backend) { unimem::Memory::global(backend); };
template<class T> concept HasBackendHeap = requires(T backend) { unimem::Memory::heap(backend); };
template<class T> concept HasBackendHeapStatistics = requires(T backend) {
    unimem::Memory::heap(backend, unimem::StatisticsMode::Disabled);
};

static_assert(!HasSelectedBackend<unimem::Memory>);
static_assert(!HasSelectedStatistics<unimem::Memory>);
static_assert(!HasRuntimeGlobalConfiguration<unimem::Memory>);
static_assert(!HasBackendGlobal<unimem::Backend>);
static_assert(!HasBackendHeap<unimem::Backend>);
static_assert(!HasBackendHeapStatistics<unimem::Backend>);
static_assert(std::is_same_v<decltype(&unimem::Memory::global), unimem::Memory& (*)()>);
static_assert(std::is_same_v<decltype(&unimem::Memory::heap),
                           unimem::Memory (*)(unimem::StatisticsMode)>);
static_assert(std::is_same_v<decltype(unimem::Memory::heap()), unimem::Memory>);
static_assert(std::is_same_v<decltype(unimem::Memory::global()), unimem::Memory&>);
}
