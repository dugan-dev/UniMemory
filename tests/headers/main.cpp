bool header_common();
bool header_memory();
bool header_owned_block();
bool header_allocator();
bool header_smart_ptr();

int main() {
    if (!header_common()) { return 1; }
    if (!header_memory()) { return 2; }
    if (!header_owned_block()) { return 3; }
    if (!header_allocator()) { return 4; }
    if (!header_smart_ptr()) { return 5; }
}
