cmake_minimum_required(VERSION 3.25)
project(UniMemoryInstalledConsumer LANGUAGES CXX)

find_package(UniMemory 0.0.1 EXACT CONFIG REQUIRED)
get_target_property(core_type UniMemory::UniMemory TYPE)
if(NOT core_type STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR "Installed UniMemory must be a header-only INTERFACE target")
endif()
foreach(property IN ITEMS BACKEND STATISTICS CHECKS_POLICY)
    get_target_property(value UniMemory::UniMemory UNIMEMORY_${property})
    if(NOT value STREQUAL UniMemory_${property})
        message(FATAL_ERROR "Installed target and package ${property} disagree")
    endif()
endforeach()
include("${CMAKE_CURRENT_SOURCE_DIR}/../configuration/requests.cmake")
unimemory_check_installed_requests(
    PACKAGE_DIR "${UniMemory_DIR}"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/profile-request-conflicts"
    BACKEND "${UniMemory_BACKEND}"
    STATISTICS "${UniMemory_STATISTICS}"
    CHECKS "${UniMemory_CHECKS_POLICY}")
if(NOT UniMemory_BACKEND MATCHES "^(standard|mimalloc|jemalloc)$" OR
   NOT UniMemory_STATISTICS MATCHES "^(ON|OFF)$" OR
   NOT UniMemory_CHECKS_POLICY MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "Missing installed build profile")
endif()
set(backends standard mimalloc jemalloc)
list(FIND backends "${UniMemory_BACKEND}" backend_number)
if(UniMemory_STATISTICS STREQUAL "ON")
    set(statistics_number 1)
else()
    set(statistics_number 0)
endif()
set(checks_policies AUTO ON OFF)
list(FIND checks_policies "${UniMemory_CHECKS_POLICY}" checks_policy_number)
math(EXPR conflicting_backend "(${backend_number} + 1) % 3")
math(EXPR conflicting_statistics "1 - ${statistics_number}")
math(EXPR conflicting_checks "(${checks_policy_number} + 1) % 3")
# The compile must fail for the intended profile conflict, not a missing SDK.
foreach(kind IN ITEMS backend statistics checks)
    if(kind STREQUAL "backend")
        set(macro UNIMEMORY_CONFIG_BACKEND)
        set(reason "backend macro disagrees")
    elseif(kind STREQUAL "statistics")
        set(macro UNIMEMORY_CONFIG_STATISTICS)
        set(reason "statistics macro disagrees")
    else()
        set(macro UNIMEMORY_CONFIG_CHECKS_POLICY)
        set(reason "checks policy disagrees")
    endif()
    try_compile(accepted SOURCE_FROM_CONTENT "conflict-${kind}.cpp"
        "#define ${macro} ${conflicting_${kind}}\n#include <unimem/memory.h>\nint main() {}\n"
        LINK_LIBRARIES UniMemory::UniMemory
        CXX_STANDARD 20 OUTPUT_VARIABLE compiler_output)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/conflict-${kind}.log" "${compiler_output}")
    if(accepted OR NOT compiler_output MATCHES "${reason}")
        message(FATAL_ERROR "Installed ${kind} conflict was not rejected for the expected reason")
    endif()
endforeach()
try_compile(accepted SOURCE_FROM_CONTENT "conflict-tuple-checks.cpp"
    "#undef UNIMEMORY_CHECKS\n#define UNIMEMORY_CHECKS (1 - UNIMEMORY_TARGET_CHECKS)\n#include <unimem/memory.h>\nint main() {}\n"
    LINK_LIBRARIES UniMemory::UniMemory CXX_STANDARD 20 OUTPUT_VARIABLE compiler_output)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/conflict-tuple-checks.log" "${compiler_output}")
if(accepted OR NOT compiler_output MATCHES "Per-TU UNIMEMORY_CHECKS override disagrees")
    message(FATAL_ERROR "Per-TU checks override was not rejected for the expected reason")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/profile.h" [=[
#pragma once
#include <unimem/memory.h>
template<class T> concept HasConfigureGlobal = requires {
    T::configure_global(T::selected_backend, T::selected_statistics);
};
static_assert(!HasConfigureGlobal<unimem::Memory>);
static_assert(static_cast<int>(unimem::Memory::selected_backend) == UNIMEMORY_CONFIG_BACKEND);
static_assert(static_cast<int>(unimem::Memory::selected_statistics) == UNIMEMORY_CONFIG_STATISTICS);
unimem::Memory* profile_a();
unimem::Memory* profile_b();
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/profile-a.cpp" [=[
#include "profile.h"
unimem::Memory* profile_a() { return &unimem::Memory::global(); }
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/profile-b.cpp" [=[
#include "profile.h"
unimem::Memory* profile_b() { return &unimem::Memory::global(unimem::Memory::selected_backend); }
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/profile-main.cpp" [=[
#include "profile.h"
#include <vector>
#include <memory_resource>
#include <stdexcept>
int main() {
    auto& memory = unimem::Memory::global();
    if (profile_a() != &memory || profile_b() != &memory) return 1;
    for (auto backend : {unimem::Backend::Standard, unimem::Backend::Mimalloc, unimem::Backend::Jemalloc}) {
        if (unimem::available(backend) != (backend == unimem::Memory::selected_backend)) return 2;
        if (backend != unimem::Memory::selected_backend) {
            try { (void)unimem::Memory::global(backend); return 3; }
            catch (const std::invalid_argument&) {}
        }
    }
    const auto before = memory.statistics();
    {
        std::vector<int, unimem::Allocator<int>> values{unimem::Allocator<int>{memory}};
        values.resize(32, 7);
        auto unique = memory.make_unique<int>(42);
        auto shared = memory.make_shared<int>(43);
        auto block = memory.make_block(128, 16);
        std::pmr::vector<int> pmr{memory.resource()};
        pmr.resize(32, 8);
        if (*unique != 42 || *shared != 43 || block.data() == nullptr) return 4;
    }
    const auto after = profile_b()->statistics();
    if constexpr (unimem::Memory::selected_statistics == unimem::StatisticsMode::Basic) {
        if (!before || !after || after->live_bytes != before->live_bytes ||
            after->allocations - before->allocations != after->deallocations - before->deallocations) return 5;
        if (after->allocations <= before->allocations) return 6;
    } else if (before || after) return 7;
}
]=])
add_executable(UniMemoryInstalledProfile
    "${CMAKE_CURRENT_BINARY_DIR}/profile-main.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/profile-a.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/profile-b.cpp")
target_link_libraries(UniMemoryInstalledProfile PRIVATE UniMemory::UniMemory)
target_compile_features(UniMemoryInstalledProfile PRIVATE cxx_std_20)
set_target_properties(UniMemoryInstalledProfile PROPERTIES CXX_EXTENSIONS OFF)
add_executable(UniMemoryInstalledConsumer main.cpp
    ../headers/common.cpp
    ../headers/memory.cpp
    ../headers/owned_block.cpp
    ../headers/allocator.cpp
    ../headers/smart_ptr.cpp
    ../headers/standalone_allocator.cpp
    ../headers/standalone_smart_ptr.cpp)
target_link_libraries(UniMemoryInstalledConsumer PRIVATE UniMemory::UniMemory)
target_compile_features(UniMemoryInstalledConsumer PRIVATE cxx_std_20)
set_target_properties(UniMemoryInstalledConsumer PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME UniMemory.installed_profile COMMAND UniMemoryInstalledProfile)
add_test(NAME UniMemory.installed_consumer COMMAND UniMemoryInstalledConsumer)
add_executable(UniMemoryInstalledPmrLifetime
    ../pmr_lifetime/main.cpp ../pmr_lifetime/a.cpp ../pmr_lifetime/b.cpp)
target_link_libraries(UniMemoryInstalledPmrLifetime PRIVATE UniMemory::UniMemory)
target_compile_features(UniMemoryInstalledPmrLifetime PRIVATE cxx_std_20)
set_target_properties(UniMemoryInstalledPmrLifetime PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME UniMemory.installed_pmr_lifetime COMMAND UniMemoryInstalledPmrLifetime)
foreach(order RANGE 0 5)
    add_executable(UniMemoryInstalledHeaderOrder${order} ../headers/include_order.cpp)
    target_compile_definitions(UniMemoryInstalledHeaderOrder${order} PRIVATE UNIMEMORY_HEADER_ORDER=${order})
    target_link_libraries(UniMemoryInstalledHeaderOrder${order} PRIVATE UniMemory::UniMemory)
    set_target_properties(UniMemoryInstalledHeaderOrder${order} PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME UniMemory.installed_header_order_${order} COMMAND UniMemoryInstalledHeaderOrder${order})
endforeach()
