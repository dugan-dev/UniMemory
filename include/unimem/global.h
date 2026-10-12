#pragma once

#include <unimem/memory.h>
#include <unimem/detail/members.h>

namespace unimem {

// Shared memory accessed through Memory::global().
class Global final : public Memory {
 private:
  UNIM_IMPL_GLOBAL_MEMBERS
};

}  // namespace unimem
