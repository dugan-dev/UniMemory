#include "compiled-test-config.h"
#include <unimem/memory.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
unimem::Backend backend(const std::string& name) {
    if (name == "standard") { return unimem::Backend::Standard; }
    if (name == "mimalloc") { return unimem::Backend::Mimalloc; }
    if (name == "jemalloc") { return unimem::Backend::Jemalloc; }
    throw std::invalid_argument("unknown backend");
}
void verify(const unimem::OwnedBlock& block, unsigned char pattern) {
    const auto* data = static_cast<const unsigned char*>(block.data());
    check(reinterpret_cast<std::uintptr_t>(data) % 64 == 0, "alignment");
    for (std::size_t offset = 0; offset < block.size(); offset += 4096) {
        check(data[offset] == pattern, "content changed across thread lifetime");
    }
    check(data[block.size() - 1] == pattern, "last byte changed");
}
void churn(unimem::Memory& memory, bool asymmetric) {
    constexpr unsigned producers = 4, batches = 32, blocks = 128;
    std::atomic<bool> failed{false};
    for (unsigned batch = 0; batch < batches; ++batch) {
        std::vector<std::vector<unimem::OwnedBlock>> queues(producers);
        std::vector<std::thread> workers;
        for (unsigned id = 0; id < producers; ++id) {
            workers.emplace_back([&, id] {
                try {
                    for (unsigned item = 0; item < blocks; ++item) {
                        auto block = memory.make_block(17 + (item * 997 + batch) % 65536, 64);
                        std::memset(block.data(), static_cast<int>(id + 1), block.size());
                        queues[id].push_back(std::move(block));
                    }
                } catch (...) { failed.store(true); }
            });
        }
        for (auto& worker : workers) { worker.join(); }
        workers.clear();
        // All producer threads have exited; surviving blocks are freed by new threads.
        const unsigned consumers = asymmetric ? 1 : producers;
        for (unsigned id = 0; id < consumers; ++id) {
            workers.emplace_back([&, id] {
                try {
                    for (unsigned queue = id; queue < producers; queue += consumers) {
                        check(queues[queue].size() == blocks, "incomplete producer batch");
                        for (auto& block : queues[queue]) {
                            verify(block, static_cast<unsigned char>(queue + 1));
                        }
                        queues[queue].clear();
                    }
                } catch (...) { failed.store(true); }
            });
        }
        for (auto& worker : workers) { worker.join(); }
        check(!failed.load(), "thread churn or asymmetric handoff failed");
    }
}
void pressure(unimem::Memory& memory) {
    constexpr std::size_t budget = 64 * 1024 * 1024;
    constexpr std::size_t count = 64, bytes = budget / count;
    std::vector<unimem::OwnedBlock> blocks;
    blocks.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        blocks.push_back(memory.make_block(0));
    }
    for (unsigned cycle = 0; cycle < 8; ++cycle) {
        for (std::size_t index = 0; index < count; ++index) {
            if (blocks[index].data()) { verify(blocks[index], 0x5A); }
            else {
                blocks[index] = memory.make_block(bytes, 64);
                std::memset(blocks[index].data(), 0x5A, bytes);
            }
        }
        check( (!compiled_test::supports_statistics || (memory.statistics()->live_bytes == budget)) , "pressure live accounting");
        for (std::size_t index = 0; index < count; ++index) {
            if (index % 8 != 0) { blocks[index] = memory.make_block(0); }
        }
        check( (!compiled_test::supports_statistics || (memory.statistics()->live_bytes == budget / 8)) , "pressure retention accounting");
    }
    blocks.clear();
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::invalid_argument("backend and mode required"); }
        const auto selected = backend(argv[1]);
        compiled_test::test_configuration(selected, compiled_test::global_mode);
        auto& memory = unimem::Memory::global(selected);
        const std::string mode = argv[2];
        if (mode == "churn") { churn(memory, false); }
        else if (mode == "asymmetric") { churn(memory, true); }
        else if (mode == "pressure") { pressure(memory); }
        else { throw std::invalid_argument("unknown workload"); }
        const auto stats = memory.statistics();
        check( (!compiled_test::supports_statistics || (stats->live_bytes == 0)) && (!compiled_test::supports_statistics || (stats->allocations == stats->deallocations)) ,
              "extended stress did not balance");
        if constexpr (compiled_test::supports_statistics) {
            std::cout << "PASS " << mode << " allocations=" << stats->allocations
                      << " peak_requested_bytes=" << stats->peak_live_bytes << '\n';
        } else { std::cout << "PASS " << mode << " statistics=disabled\n"; }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
