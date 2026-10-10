#include <unimem/memory.h>

#include "api.h"
#include "backend.h"
#include "hotpath.h"
#include "statistics.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

namespace {
bool pin_single() {
#ifdef _WIN32
    DWORD_PTR process_mask = 0, system_mask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) || process_mask == 0) { return false; }
    const auto first = process_mask & (~process_mask + 1);
    return SetProcessAffinityMask(GetCurrentProcess(), first) != 0;
#elif defined(__linux__)
    cpu_set_t allowed;
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) { return false; }
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (CPU_ISSET(cpu, &allowed)) {
            cpu_set_t selected; CPU_ZERO(&selected); CPU_SET(cpu, &selected);
            return sched_setaffinity(0, sizeof(selected), &selected) == 0;
        }
    }
    return false;
#else
    return false;
#endif
}

void clock_samples(std::ostream& out, std::size_t iterations) {
    using Clock = std::chrono::steady_clock;
    std::vector<double> samples;
    samples.reserve(iterations);
    const auto start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto a = Clock::now();
        const auto b = Clock::now();
        samples.push_back(std::chrono::duration<double,std::nano>(b-a).count());
    }
    const auto seconds = std::chrono::duration<double>(Clock::now()-start).count();
    std::sort(samples.begin(),samples.end());
    for (const auto fraction : {0.5,0.99,0.999}) {
        const auto index = static_cast<std::size_t>(fraction * static_cast<double>(samples.size()-1));
        out << "clock,none,p" << fraction*100 << ",0,1," << iterations << ',' << seconds << ','
            << samples[index] << ",ns/sample,0\n";
    }
}
}

int main(int argc, char** argv) {
    try {
        std::string suite="hotpath", backend="standard", variant="native", affinity="none";
        std::size_t iterations=10000, threads=1, bytes=64;
        bool touch=false;
        if ((argc-1)%2 != 0) { throw std::invalid_argument("expected named value pairs"); }
        for (int i=1; i<argc; i+=2) {
            const std::string option=argv[i], value=argv[i+1];
            if (option=="--suite") { suite=value; }
            else if (option=="--backend") { backend=value; }
            else if (option=="--variant") { variant=value; }
            else if (option=="--affinity") { affinity=value; }
            else if (option=="--touch") { if (value!="0" && value!="1") { throw std::invalid_argument("touch"); } touch=value=="1"; }
            else {
                std::size_t used=0;
                const auto number=std::stoull(value,&used);
                if (used!=value.size() || value.starts_with('-') || number==0 || number>50000000) { throw std::invalid_argument("invalid positive count"); }
                if (option=="--iterations") { iterations=static_cast<std::size_t>(number); }
                else if (option=="--threads") { threads=static_cast<std::size_t>(number); }
                else if (option=="--bytes") { bytes=static_cast<std::size_t>(number); }
                else { throw std::invalid_argument("unknown option"); }
            }
        }
        if (threads>16 || bytes>65536 || (affinity!="none" && affinity!="single")) { throw std::invalid_argument("unsupported configuration"); }
        if (affinity=="single" && !pin_single()) { throw std::runtime_error("requested CPU affinity unavailable"); }
        std::cout << "suite,backend,variant,bytes,threads,operations,seconds,value,unit,checksum\n";
        if (suite=="hotpath") { unimem_diagnostics::run_hotpath(std::cout,backend,variant,iterations,bytes,touch); }
        else if (suite=="statistics") { unimem_diagnostics::run_statistics(std::cout,backend,variant,iterations,threads); }
        else if (suite=="backend") { unimem_diagnostics::run_backend(std::cout,variant,backend,iterations); }
        else if (suite=="api") { unimem_diagnostics::run_api(std::cout,backend,variant,iterations); }
        else if (suite=="clock") { clock_samples(std::cout,iterations); }
        else { throw std::invalid_argument("unknown suite"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "diagnostic failure: " << error.what() << '\n';
        return 1;
    }
}
