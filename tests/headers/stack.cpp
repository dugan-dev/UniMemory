#include <unimem/stack.h>
#include <type_traits>

static_assert(std::is_base_of_v<unimem::Memory, unimem::Stack>);
static_assert(std::is_final_v<unimem::Stack>);
static_assert(!std::is_abstract_v<unimem::Stack>);
static_assert(std::has_virtual_destructor_v<unimem::Stack>);
static_assert(std::is_constructible_v<unimem::Stack, void*, std::size_t>);
static_assert(std::is_constructible_v<unimem::Stack, std::span<std::byte>>);
static_assert(!std::is_copy_constructible_v<unimem::Stack>);
static_assert(!std::is_move_constructible_v<unimem::Stack>);
static_assert(std::is_same_v<decltype(std::declval<const unimem::Stack&>().mark()), unimem::Stack::Mark>);
static_assert(requires(unimem::Stack& stack, unimem::Stack::Mark mark) {
  stack.reset();
  stack.rewind(mark);
  stack.used();
  stack.capacity();
});
