#include <limits>

int main(int argc, char**) {
    volatile int value = std::numeric_limits<int>::max();
    // Isolated CI control: UBSan must reject this instead of returning success.
    volatile int overflow = value + argc;
    (void)overflow;
    return 0;
}
