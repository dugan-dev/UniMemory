#include <unimem/memory.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
using namespace unimem;
constexpr auto backend = Backend::Jemalloc;

void check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void allocate(Memory& memory, unsigned loops) {
    for (unsigned loop = 0; loop < loops; ++loop) {
        std::array<std::optional<OwnedBlock>, 64> blocks;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto bytes = 48 + ((i + loop) % 16) * 24;
            blocks[i].emplace(memory.make_block(bytes, 8));
            std::memset(blocks[i]->data(), 0x35, bytes);
        }
        for (const auto& block : blocks) {
            const auto* bytes = static_cast<const unsigned char*>(block->data());
            check(bytes[0] == 0x35 && bytes[block->size() - 1] == 0x35,
                  "concurrent allocation data changed");
        }
    }
}

void worker(std::string_view mode, Memory* shared, unsigned index, unsigned loops) {
    if (mode == "mixed") {
        constexpr std::array modes{"global", "heap", "capabilities", "option"};
        mode = modes[index % modes.size()];
    }
    if (mode == "shared") {
        allocate(*shared, loops);
    } else if (mode == "heap") {
        auto memory = Memory::heap(backend);
        allocate(memory, loops);
        auto block = memory.make_block(128, 64);
        check(memory.owns(block.data()), "heap allocation lost its arena");
    } else {
        if (mode == "capabilities") {
            check(capabilities(backend).release_delay, "jemalloc capabilities unavailable");
        } else if (mode == "option") {
            check(set_runtime_option(backend, RuntimeOption::UnusedPageReleaseDelayMs, 10),
                  "jemalloc runtime option unavailable");
        }
        allocate(Memory::global(backend), loops);
    }
}
}

int main(int argc, char** argv) {
    try {
        check(argc >= 2 && argc <= 4, "expected mode [rounds [loops]]");
        const std::string_view mode = argv[1];
        check(mode == "shared" || mode == "global" || mode == "heap" ||
              mode == "capabilities" || mode == "option" || mode == "mixed",
              "unknown initialization mode");
        const auto rounds = argc > 2 ? std::stoul(argv[2]) : 1;
        const auto loops = argc > 3 ? std::stoul(argv[3]) : 1;
        check(rounds >= 1 && rounds <= 64 && loops >= 1 && loops <= 64,
              "invalid workload size");
        // Only the shared case constructs Memory before starting its workers.
        // No jemalloc query or allocator warmup precedes the other cold entries.
        auto* shared = mode == "shared" ? &Memory::global(backend) : nullptr;
        for (unsigned long round = 0; round < rounds; ++round) {
            std::array<std::exception_ptr, 6> failures;
            {
                std::array<std::thread, 6> workers;
                struct JoinWorkers {
                    std::array<std::thread, 6>& workers;
                    ~JoinWorkers() {
                        for (auto& thread : workers) {
                            if (thread.joinable()) { thread.join(); }
                        }
                    }
                } join_workers{workers};
                for (unsigned index = 0; index < workers.size(); ++index) {
                    workers[index] = std::thread([&, index] {
                        try { worker(mode, shared, index, static_cast<unsigned>(loops)); }
                        catch (...) { failures[index] = std::current_exception(); }
                    });
                }
            }
            for (const auto& failure : failures) {
                if (failure) { std::rethrow_exception(failure); }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
