#include <unimem/heap.h>
#include <type_traits>

static_assert(std::is_base_of_v<unimem::Memory, unimem::Heap>);
static_assert(std::is_final_v<unimem::Heap>);
static_assert(!std::is_abstract_v<unimem::Heap>);
static_assert(!std::has_virtual_destructor_v<unimem::Heap>);
static_assert(!std::is_polymorphic_v<unimem::Heap>);
static_assert(std::is_constructible_v<unimem::Heap, unimem::StatisticsMode>);
static_assert(!std::is_copy_constructible_v<unimem::Heap>);
static_assert(!std::is_move_constructible_v<unimem::Heap>);
static_assert(requires(unimem::Heap& heap) {
  heap.reset();
  heap.collect();
  heap.owns(nullptr);
});
