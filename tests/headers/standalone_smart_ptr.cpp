#include <unimem/smart_ptr.h>
#include "standalone_types.h"

bool standalone_smart_ptr(unimem::Memory& memory, StandaloneObject* object,
                          StandaloneArrayElement* array) {
    unimem::Unique<StandaloneObject> owner(object, {&memory});
    unimem::UniqueArray<StandaloneArrayElement> array_owner(array, {&memory, 3});
    return owner->value == 17 && array_owner[2].value == 23;
}
