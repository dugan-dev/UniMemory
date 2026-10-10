#pragma once

#include <unimem/common.h>
#include <unimem/detail/backend.h>

#include <cstddef>
#include <ostream>
#include <string>

namespace unimem_diagnostics {

const unimem::detail::BackendOps& shim_ops(unimem::Backend backend);
void* shim_standard_allocate(void*, std::size_t, std::size_t) noexcept;
void shim_standard_free(void*, void*, std::size_t, std::size_t) noexcept;
void* shim_mimalloc_allocate(void*, std::size_t, std::size_t) noexcept;
void shim_mimalloc_free(void*, void*, std::size_t, std::size_t) noexcept;
void* shim_jemalloc_allocate(void*, std::size_t, std::size_t) noexcept;
void* shim_jemalloc_allocate_bits(void*, std::size_t, std::size_t) noexcept;
void shim_jemalloc_free(void*, void*, std::size_t, std::size_t) noexcept;
void* external_checked_allocate(const unimem::detail::BackendHandle&, std::size_t, std::size_t);
void external_checked_free(const unimem::detail::BackendHandle&, void*, std::size_t, std::size_t) noexcept;
void* external_unchecked_allocate(const unimem::detail::BackendHandle&, std::size_t, std::size_t);
void external_unchecked_free(const unimem::detail::BackendHandle&, void*, std::size_t, std::size_t) noexcept;
std::size_t opaque_alignment(std::size_t) noexcept;
void verify_jemalloc_bit_flags();
void run_hotpath(std::ostream&, const std::string&, const std::string&, std::size_t, std::size_t, bool);

}
