#pragma once
#include <unimem/common.h>
#include <type_traits>

namespace test_api {
template<class T> concept HasAvailableMember = requires(T value) { value.available; };
template<class T> concept HasBackendAvailability = requires(T backend) { available(backend); };
template<class T> concept HasBackendCapabilities = requires(T backend) { unimem::capabilities(backend); };
template<class T> concept HasBackendSupports = requires(T backend) {
    unimem::supports(backend, unimem::RuntimeOption::UnusedPageReleaseDelayMs);
};
template<class T> concept HasBackendRuntimeOption = requires(T backend) {
    unimem::set_runtime_option(backend, unimem::RuntimeOption::UnusedPageReleaseDelayMs, 10);
};

static_assert(!HasAvailableMember<unimem::BackendCapabilities>);
static_assert(!HasBackendAvailability<unimem::Backend>);
static_assert(!HasBackendCapabilities<unimem::Backend>);
static_assert(!HasBackendSupports<unimem::Backend>);
static_assert(!HasBackendRuntimeOption<unimem::Backend>);
static_assert(std::is_same_v<decltype(&unimem::capabilities),
                           unimem::BackendCapabilities (*)() noexcept>);
static_assert(std::is_same_v<decltype(&unimem::supports),
                           bool (*)(unimem::RuntimeOption) noexcept>);
static_assert(std::is_same_v<decltype(&unimem::set_runtime_option),
                           bool (*)(unimem::RuntimeOption, std::int64_t)>);
}
