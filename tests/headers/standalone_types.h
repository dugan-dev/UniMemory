#pragma once

namespace unimem { class Memory; }

struct StandaloneObject { int value; };
struct StandaloneArrayElement { int value; };

bool standalone_allocator(unimem::Memory& memory);
bool standalone_smart_ptr(unimem::Memory& memory, StandaloneObject* object,
                          StandaloneArrayElement* array);
