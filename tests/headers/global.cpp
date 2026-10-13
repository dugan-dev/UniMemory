#include <unimem/global.h>
#include <type_traits>

static_assert(std::is_base_of_v<unimem::Memory, unimem::Global>);
static_assert(std::is_final_v<unimem::Global>);
static_assert(!std::is_abstract_v<unimem::Global>);
static_assert(!std::has_virtual_destructor_v<unimem::Global>);
static_assert(!std::is_polymorphic_v<unimem::Global>);
static_assert(std::is_default_constructible_v<unimem::Global>);
static_assert(std::is_destructible_v<unimem::Global>);
static_assert(std::is_constructible_v<unimem::Global, unimem::StatisticsMode>);
static_assert(!std::is_copy_constructible_v<unimem::Global>);
static_assert(!std::is_move_constructible_v<unimem::Global>);
