#include <unimem/memory.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <type_traits>

using namespace unimem;

static_assert(std::is_default_constructible_v<Unique<int>>);
static_assert(std::is_default_constructible_v<UniqueArray<int>>);
static_assert(sizeof(Deleter<int>) == sizeof(Memory*));
static_assert(!std::is_polymorphic_v<Memory>);

namespace {
struct Object { static inline unsigned alive = 0; Object() { ++alive; } ~Object() noexcept { --alive; } };
void check(bool condition) {
    if (!condition) { throw std::runtime_error("deleter contract check failed"); }
}
}

int main(int argc, char** argv) {
    const std::string_view mode = argc == 2 ? argv[1] : "valid";
    if (mode != "valid") {
        std::set_terminate([] { std::_Exit(93); });
        Memory& memory = Memory::global();
        if (mode == "unbound_object") { Deleter<int>{}(memory.create<int>(7)); }
        else if (mode == "unbound_array") { ArrayDeleter<int>{}(memory.create_array<int>(3)); }
        else if (mode == "zero_array_count") {
            ArrayDeleter<int>{&memory, 0}(memory.create_array<int>(3));
        } else { return 2; }
        return 3;
    }
    try {
        Deleter<int>{}(nullptr);
        ArrayDeleter<int>{}(nullptr);
        Unique<Object> empty;
        UniqueArray<Object> empty_array;
        check(!empty && !empty_array);
        Memory& memory = Memory::global();
        Object* raw = memory.create<Object>();
        {
            auto owner = memory.adopt_unique(raw);
            check(owner.get() == raw && Object::alive == 1);
            auto replacement = memory.adopt_unique(owner.release());
            check(!owner && replacement.get() == raw);
            empty = std::move(replacement);
            check(!replacement && empty.get_deleter().memory == &memory);
        }
        empty.reset();
        check(Object::alive == 0);
        {
            auto owner = memory.adopt_unique_array(memory.create_array<Object>(3), 3);
            check(owner.get_deleter().count == 3 && Object::alive == 3);
            owner = memory.adopt_unique_array(memory.create_array<Object>(5), 5);
            check(owner.get_deleter().count == 5 && Object::alive == 5);
            empty_array = std::move(owner);
        }
        empty_array.reset();
        check(Object::alive == 0);
        auto null_object = memory.adopt_unique(static_cast<Object*>(nullptr));
        auto null_array = memory.adopt_unique_array(static_cast<Object*>(nullptr), 0);
        check(!null_object && !null_array);
        std::cout << "Owner binding and empty-state contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
