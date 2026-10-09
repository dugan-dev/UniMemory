#include <fstream>
#include <iostream>
#include <string>

// Deterministic process boundary for native-symbol validation error paths.
int main(int argc, char** argv) {
    if (argc < 2) { return 2; }
    std::ifstream input(argv[argc - 1]);
    std::string kind;
    input >> kind;
    if (kind == "error") {
        std::cerr << "fixture: cannot read symbols\n";
        return 1;
    }
    if (kind == "replace") { std::cout << "00000000 T _Znwm\n"; }
    return input ? 0 : 2;
}
