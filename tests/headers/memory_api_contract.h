#pragma once
#include <unimem/all.h>
#include "types_api_contract.h"

namespace test_api {
template <class T>
concept HasSelectedBackend = requires { T::selected_backend; };
template <class T>
concept HasSelectedStatistics = requires { T::selected_statistics; };
template <class T>
concept HasRuntimeGlobalConfiguration = requires { T::configure_global(0, unimem::StatisticsMode::Disabled); };
template <class T>
concept HasBackendGlobal = requires(T backend) { unimem::Memory::global(backend); };
template <class T>
concept HasBackendHeap = requires(T backend) { unimem::Memory::heap(backend); };
template <class T>
concept HasBackendHeapStatistics = requires(T backend) { unimem::Memory::heap(backend, unimem::StatisticsMode::Disabled); };
template <class T>
concept HasConfig = requires { T::config(0); };
template <class T>
concept HasBackend = requires(T& memory) { memory.backend(); };
template <class T>
concept HasCapabilities = requires(T& memory) { memory.capabilities(); };
template <class T>
concept HasBackendStatistics = requires(T& memory) { memory.backend_statistics(); };

static_assert(!HasSelectedBackend<unimem::Memory>);
static_assert(!HasSelectedStatistics<unimem::Memory>);
static_assert(!HasRuntimeGlobalConfiguration<unimem::Memory>);
static_assert(!HasBackendGlobal<int>);
static_assert(!HasBackendHeap<int>);
static_assert(!HasBackendHeapStatistics<int>);
static_assert(HasConfig<unimem::Memory>);
static_assert(HasBackendHeap<unimem::StatisticsMode>);
static_assert(HasBackendGlobal<unimem::StatisticsMode>);
static_assert(std::is_same_v<decltype(unimem::Memory::stack(nullptr, 0, unimem::StatisticsMode::Disabled)), unimem::Stack>);
static_assert(std::is_same_v<decltype(&unimem::Memory::config), void (*)(std::int64_t)>);
static_assert(std::is_same_v<decltype(std::declval<unimem::Memory&>().set_statistics(unimem::StatisticsMode::Basic)), void>);
static_assert(!HasBackend<unimem::Memory>);
static_assert(std::is_same_v<decltype(std::declval<const unimem::Memory&>().statistics()), unimem::Statistics>);
static_assert(std::is_same_v<decltype(std::declval<unimem::Memory&>().reset_statistics()), void>);
static_assert(!HasCapabilities<unimem::Memory>);
static_assert(!HasBackendStatistics<unimem::Memory>);
static_assert(std::is_same_v<decltype(&unimem::Memory::global), unimem::Global& (*)(unimem::StatisticsMode)>);
static_assert(std::is_same_v<decltype(&unimem::Memory::heap), unimem::Heap (*)(unimem::StatisticsMode)>);
static_assert(std::is_same_v<decltype(unimem::Memory::heap()), unimem::Heap>);
static_assert(std::is_same_v<decltype(unimem::Memory::global()), unimem::Global&>);

template <class T>
concept HasInstance = requires { T::instance(); };
static_assert(!HasInstance<unimem::Global>);

// Dedicated operations are rejected by the type system, before any runtime call.
template <class T>
concept HasReset = requires(T& memory) { memory.reset(); };
template <class T>
concept HasCollect = requires(T& memory) { memory.collect(); };
template <class T>
concept HasOwns = requires(const T& memory) { memory.owns(nullptr); };
template <class T>
concept HasMark = requires(const T& memory) { memory.mark(); };
template <class T>
concept HasMarkType = requires { typename T::Mark; };
template <class T>
concept HasRewind = requires(T& memory, unimem::Stack::Mark mark) { memory.rewind(mark); };
template <class T>
concept HasUsed = requires(const T& memory) { memory.used(); };
template <class T>
concept HasCapacity = requires(const T& memory) { memory.capacity(); };
template <class T>
constexpr bool no_dedicated_operations =
    !HasReset<T> && !HasCollect<T> && !HasOwns<T> && !HasMark<T> && !HasMarkType<T> && !HasRewind<T> && !HasUsed<T> && !HasCapacity<T>;
static_assert(no_dedicated_operations<unimem::Memory>);
static_assert(no_dedicated_operations<unimem::Global>);
static_assert(HasReset<unimem::Heap> && HasCollect<unimem::Heap> && HasOwns<unimem::Heap>);
static_assert(!HasMark<unimem::Heap> && !HasMarkType<unimem::Heap> && !HasRewind<unimem::Heap> && !HasUsed<unimem::Heap> &&
              !HasCapacity<unimem::Heap>);
static_assert(HasReset<unimem::Stack> && HasMark<unimem::Stack> && HasMarkType<unimem::Stack> && HasRewind<unimem::Stack> && HasUsed<unimem::Stack> &&
              HasCapacity<unimem::Stack>);
static_assert(!HasCollect<unimem::Stack> && !HasOwns<unimem::Stack>);
static_assert(std::is_abstract_v<unimem::Memory> && std::is_polymorphic_v<unimem::Memory>);
static_assert(std::has_virtual_destructor_v<unimem::Memory>);
static_assert(!std::is_copy_constructible_v<unimem::Memory> && !std::is_move_constructible_v<unimem::Memory>);
static_assert(!std::is_copy_assignable_v<unimem::Memory> && !std::is_move_assignable_v<unimem::Memory>);
static_assert(std::is_base_of_v<unimem::Memory, unimem::Global> && std::is_final_v<unimem::Global>);
static_assert(std::is_base_of_v<unimem::Memory, unimem::Heap> && std::is_final_v<unimem::Heap>);
static_assert(std::is_base_of_v<unimem::Memory, unimem::Stack> && std::is_final_v<unimem::Stack>);
template <class T>
constexpr bool concrete_memory_contract =
    !std::is_abstract_v<T> && std::is_final_v<T> && std::has_virtual_destructor_v<T> && !std::is_copy_constructible_v<T> &&
    !std::is_move_constructible_v<T> && !std::is_copy_assignable_v<T> && !std::is_move_assignable_v<T>;
static_assert(concrete_memory_contract<unimem::Global>);
static_assert(concrete_memory_contract<unimem::Heap>);
static_assert(concrete_memory_contract<unimem::Stack>);
static_assert(std::is_convertible_v<unimem::Global*, unimem::Memory*>);
static_assert(std::is_convertible_v<unimem::Heap*, unimem::Memory*>);
static_assert(std::is_convertible_v<unimem::Stack*, unimem::Memory*>);
static_assert(!std::is_default_constructible_v<unimem::Global>);
static_assert(!std::is_constructible_v<unimem::Global, unimem::StatisticsMode>);
static_assert(std::is_default_constructible_v<unimem::Heap>);
static_assert(std::is_constructible_v<unimem::Heap, unimem::StatisticsMode>);
static_assert(std::is_convertible_v<unimem::StatisticsMode, unimem::Heap>);
static_assert(std::is_constructible_v<unimem::Stack, void*, std::size_t, unimem::StatisticsMode>);
static_assert(std::is_constructible_v<unimem::Stack, std::span<std::byte>, unimem::StatisticsMode>);
static_assert(std::is_same_v<decltype(&unimem::Memory::allocate), void* (unimem::Memory::*)(std::size_t, std::size_t)>);
static_assert(std::is_same_v<decltype(&unimem::Memory::allocate_zeroed), void* (unimem::Memory::*)(std::size_t, std::size_t)>);
static_assert(std::is_same_v<decltype(&unimem::Memory::reallocate), void* (unimem::Memory::*)(void*, std::size_t, std::size_t, std::size_t)>);
static_assert(std::is_same_v<decltype(&unimem::Memory::deallocate), void (unimem::Memory::*)(void*, std::size_t, std::size_t) noexcept>);
}  // namespace test_api

static_assert(std::is_same_v<unimem::Stack::Mark, unimem::detail::StackMark>);
