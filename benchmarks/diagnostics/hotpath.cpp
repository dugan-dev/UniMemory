#include "compiled-bench-config.h"
#include <unimem/memory.h>

#include "hotpath.h"

#include <chrono>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <new>
#include <stdexcept>

#ifdef UNIMEMORY_BENCH_MIMALLOC
#include <mimalloc.h>
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
#define JEMALLOC_NO_RENAME
#include <jemalloc/jemalloc.h>
#endif
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace unimem_diagnostics {
namespace {
using unimem::Backend;
using unimem::Memory;
using unimem::StatisticsMode;
using Handle = unimem::detail::BackendHandle;
using Clock = std::chrono::steady_clock;

inline void escape(void* pointer) noexcept {
#ifdef _MSC_VER
    static void* volatile sink;
    sink = pointer;
    _ReadWriteBarrier();
#else
    asm volatile("" : : "g"(pointer) : "memory");
#endif
}

Backend parse_backend(const std::string& name) {
    if (name == "standard") { return Backend::Standard; }
    if (name == "mimalloc") { return Backend::Mimalloc; }
    if (name == "jemalloc") { return Backend::Jemalloc; }
    throw std::invalid_argument("unknown backend");
}

Handle real_handle(Backend backend) {
    switch (backend) {
    case Backend::Standard: return unimem::detail::system_backend(false);
#ifdef UNIMEMORY_BENCH_MIMALLOC
    case Backend::Mimalloc: return unimem::detail::mimalloc_backend(false);
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
    case Backend::Jemalloc: return unimem::detail::jemalloc_backend(false);
#endif
    default: throw std::invalid_argument("backend not enabled");
    }
}

template<Backend B, bool Aligned = true, bool Throwing = false>
struct Native {
    void* allocate(std::size_t bytes, std::size_t alignment) {
        void* pointer = nullptr;
        if constexpr (B == Backend::Standard) {
            if constexpr (Throwing) { pointer = ::operator new(bytes, std::align_val_t(alignment)); }
            else if constexpr (Aligned) { pointer = ::operator new(bytes, std::align_val_t(alignment), std::nothrow); }
            else { pointer = ::operator new(bytes, std::nothrow); }
        }
#ifdef UNIMEMORY_BENCH_MIMALLOC
        else if constexpr (B == Backend::Mimalloc) {
            pointer = Aligned ? mi_malloc_aligned(bytes, alignment) : mi_malloc(bytes);
        }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
        else if constexpr (B == Backend::Jemalloc) {
            pointer = je_mallocx(bytes, Aligned ? MALLOCX_ALIGN(alignment) : 0);
        }
#endif
        if (!pointer) { throw std::bad_alloc(); }
        return pointer;
    }
    void deallocate(void* pointer, std::size_t, std::size_t alignment) noexcept {
        if constexpr (B == Backend::Standard) {
            if constexpr (Aligned || Throwing) { ::operator delete(pointer, std::align_val_t(alignment)); }
            else { ::operator delete(pointer); }
        }
#ifdef UNIMEMORY_BENCH_MIMALLOC
        else if constexpr (B == Backend::Mimalloc) { mi_free(pointer); }
#endif
#ifdef UNIMEMORY_BENCH_JEMALLOC
        else if constexpr (B == Backend::Jemalloc) { je_dallocx(pointer, 0); }
#endif
    }
};

template<Backend B>
struct DirectAdapter {
    void* allocate(std::size_t bytes, std::size_t alignment) {
        void* pointer = nullptr;
        if constexpr (B == Backend::Standard) { pointer = shim_standard_allocate(nullptr, bytes, alignment); }
        else if constexpr (B == Backend::Mimalloc) { pointer = shim_mimalloc_allocate(nullptr, bytes, alignment); }
        else { pointer = shim_jemalloc_allocate(nullptr, bytes, alignment); }
        if (!pointer) { throw std::bad_alloc(); }
        return pointer;
    }
    void deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
        if constexpr (B == Backend::Standard) { shim_standard_free(nullptr, pointer, bytes, alignment); }
        else if constexpr (B == Backend::Mimalloc) { shim_mimalloc_free(nullptr, pointer, bytes, alignment); }
        else { shim_jemalloc_free(nullptr, pointer, bytes, alignment); }
    }
};

template<bool Checked, bool External>
struct HandlePolicy {
    Handle handle;
    void* allocate(std::size_t bytes, std::size_t alignment) {
        if constexpr (External) {
            if constexpr (Checked) { return external_checked_allocate(handle, bytes, alignment); }
            else { return external_unchecked_allocate(handle, bytes, alignment); }
        } else {
            if constexpr (Checked) {
                if (alignment == 0 || (alignment & (alignment - 1)) != 0) { throw std::invalid_argument("alignment"); }
                if (bytes == 0) { return nullptr; }
            }
            void* pointer = handle.ops->allocate(handle.context, bytes, alignment);
            if (!pointer) { throw std::bad_alloc(); }
            return pointer;
        }
    }
    void deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
        if constexpr (External) {
            if constexpr (Checked) { external_checked_free(handle, pointer, bytes, alignment); }
            else { external_unchecked_free(handle, pointer, bytes, alignment); }
        } else {
            if constexpr (Checked) { if (!pointer) { return; } }
            handle.ops->deallocate(handle.context, pointer, bytes, alignment);
        }
    }
};

struct BitsAdapter {
    BitsAdapter() { verify_jemalloc_bit_flags(); }
    void* allocate(std::size_t bytes,std::size_t alignment) {
        void* pointer=shim_jemalloc_allocate_bits(nullptr,bytes,alignment);
        if (!pointer) { throw std::bad_alloc(); }
        return pointer;
    }
    void deallocate(void* pointer,std::size_t bytes,std::size_t alignment) noexcept {
        shim_jemalloc_free(nullptr,pointer,bytes,alignment);
    }
};

template<bool Lookup>
struct Actual {
    Backend backend;
    Memory& memory;
    void* allocate(std::size_t bytes, std::size_t alignment) {
        if constexpr (Lookup) { return Memory::global(backend).allocate(bytes, alignment); }
        else { return memory.allocate(bytes, alignment); }
    }
    void deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
        if constexpr (Lookup) { Memory::global(backend).deallocate(pointer, bytes, alignment); }
        else { memory.deallocate(pointer, bytes, alignment); }
    }
};

template<class Policy, bool Constant = false>
void kernel(std::ostream& out, const std::string& backend, const std::string& variant,
            Policy policy, std::size_t iterations, std::size_t runtime_bytes, bool touch,
            std::size_t runtime_alignment = 16) {
    const auto bytes = Constant ? std::size_t{64} : runtime_bytes;
    const auto alignment = Constant ? std::size_t{16} : runtime_alignment;
    for (std::size_t i = 0; i < 256; ++i) {
        void* pointer = policy.allocate(bytes, alignment);
        if (!pointer || reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) { throw std::runtime_error("allocation contract failed"); }
        escape(pointer);
        policy.deallocate(pointer, bytes, alignment);
    }
    std::uint64_t checksum = 0;
    const auto start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        void* pointer = policy.allocate(bytes, alignment);
        escape(pointer);
        if (touch) {
            std::memset(pointer, static_cast<int>(i & 255), bytes);
            checksum += static_cast<unsigned char*>(pointer)[bytes - 1];
        } else { checksum += reinterpret_cast<std::uintptr_t>(pointer) & 255; }
        policy.deallocate(pointer, bytes, alignment);
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    if (seconds <= 0) { throw std::runtime_error("nonpositive duration"); }
    out << "hotpath," << backend << ',' << variant << ',' << bytes << ",1," << iterations << ','
        << std::setprecision(12) << seconds << ',' << seconds * 1e9 / static_cast<double>(iterations)
        << ",ns/pair," << checksum << '\n';
}

template<class Policy>
void batched(std::ostream& out, const std::string& backend, const std::string& variant,
             Policy policy, std::size_t iterations, std::size_t bytes) {
    std::array<void*,64> pointers{};
    for (std::size_t i=0;i<256;++i) {
        void* p=policy.allocate(bytes,16);
        if (!p || reinterpret_cast<std::uintptr_t>(p)%16!=0) { throw std::runtime_error("batch alignment"); }
        policy.deallocate(p,bytes,16);
    }
    std::uint64_t checksum=0;
    const auto start=Clock::now();
    for (std::size_t done=0;done<iterations;) {
        const auto count=std::min(pointers.size(),iterations-done);
        for (std::size_t i=0;i<count;++i) {
            pointers[i]=policy.allocate(bytes,16);
            escape(pointers[i]);
            checksum+=reinterpret_cast<std::uintptr_t>(pointers[i])&255;
        }
        for (std::size_t i=0;i<count;++i) {
            policy.deallocate(pointers[i],bytes,16);
        }
        done+=count;
    }
    const double seconds=std::chrono::duration<double>(Clock::now()-start).count();
    if (seconds<=0) { throw std::runtime_error("batch duration"); }
    out<<"hotpath,"<<backend<<','<<variant<<','<<bytes<<",1,"<<iterations<<','<<std::setprecision(12)
       <<seconds<<','<<seconds*1e9/static_cast<double>(iterations)<<",ns/pair,"<<checksum<<'\n';
}

template<Backend B>
void selected(std::ostream& out, const std::string& backend, const std::string& variant,
              std::size_t iterations, std::size_t bytes, bool touch) {
    if (variant == "native") { kernel(out, backend, variant, Native<B>{}, iterations, bytes, touch); }
    else if (variant == "native_runtime_alignment") { kernel(out,backend,variant,Native<B>{},iterations,bytes,touch,opaque_alignment(16)); }
    else if (variant == "native_constant") { kernel<Native<B>,true>(out,backend,variant,Native<B>{},iterations,64,touch); }
    else if (variant == "native_unaligned") { kernel(out, backend, variant, Native<B,false>{}, iterations, bytes, touch); }
    else if (variant == "native_throwing") {
        if constexpr (B == Backend::Standard) { kernel(out, backend, variant, Native<B,true,true>{}, iterations, bytes, touch); }
        else { throw std::invalid_argument("throwing native is Standard only"); }
    }
    else if (variant == "adapter_direct") { kernel(out, backend, variant, DirectAdapter<B>{}, iterations, bytes, touch); }
    else if (variant == "adapter_cpp20_bits") {
        if constexpr (B==Backend::Jemalloc) { kernel(out,backend,variant,BitsAdapter{},iterations,bytes,touch); }
        else { throw std::invalid_argument("bit flags probe is jemalloc only"); }
    }
    else if (variant == "adapter_pointer") { kernel(out, backend, variant, HandlePolicy<false,false>{{&shim_ops(B),nullptr}}, iterations, bytes, touch); }
    else if (variant == "handle_inline_unchecked") { kernel(out,backend,variant,HandlePolicy<false,false>{real_handle(B)},iterations,bytes,touch); }
    else if (variant == "handle_inline_checked") { kernel(out,backend,variant,HandlePolicy<true,false>{real_handle(B)},iterations,bytes,touch); }
    else if (variant == "handle_runtime_alignment") { kernel(out,backend,variant,HandlePolicy<true,false>{real_handle(B)},iterations,bytes,touch,opaque_alignment(16)); }
    else if (variant == "handle_external_unchecked") { kernel(out,backend,variant,HandlePolicy<false,true>{real_handle(B)},iterations,bytes,touch); }
    else if (variant == "handle_external_checked") { kernel(out,backend,variant,HandlePolicy<true,true>{real_handle(B)},iterations,bytes,touch); }
    else if (variant == "handle_checked_constant") { kernel<HandlePolicy<true,false>,true>(out,backend,variant,HandlePolicy<true,false>{real_handle(B)},iterations,64,touch); }
    else if (variant == "api_cached" || variant == "api_lookup") {
        compiled_benchmark_configuration(B,StatisticsMode::Disabled);
        auto& memory = Memory::global(B);
        if (variant == "api_cached") { kernel(out,backend,variant,Actual<false>{B,memory},iterations,bytes,touch); }
        else { kernel(out,backend,variant,Actual<true>{B,memory},iterations,bytes,touch); }
    }
    else if (variant=="native_batch") { batched(out,backend,variant,Native<B>{},iterations,bytes); }
    else if (variant=="api_batch") {
        compiled_benchmark_configuration(B,StatisticsMode::Disabled);
        batched(out,backend,variant,Actual<false>{B,Memory::global(B)},iterations,bytes);
    }
    else { throw std::invalid_argument("unknown hotpath variant"); }
}
}

void run_hotpath(std::ostream& out, const std::string& backend, const std::string& variant,
                 std::size_t iterations, std::size_t bytes, bool touch) {
    if (iterations == 0 || bytes == 0 || bytes > 65536) { throw std::invalid_argument("invalid work size"); }
    const auto selected_backend = parse_backend(backend);
    if (!unimem::available(selected_backend)) { throw std::invalid_argument("disabled backend"); }
    switch (selected_backend) {
    case Backend::Standard: selected<Backend::Standard>(out,backend,variant,iterations,bytes,touch); break;
    case Backend::Mimalloc: selected<Backend::Mimalloc>(out,backend,variant,iterations,bytes,touch); break;
    case Backend::Jemalloc: selected<Backend::Jemalloc>(out,backend,variant,iterations,bytes,touch); break;
    }
}

}
