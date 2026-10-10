cmake_minimum_required(VERSION 3.25)

# Select the backend and statistics once for every consumer of this target.
if(NOT DEFINED UNIMEMORY_BACKEND OR NOT UNIMEMORY_BACKEND MATCHES "^(standard|mimalloc|jemalloc)$")
    message(FATAL_ERROR "Set UNIMEMORY_BACKEND explicitly to standard/mimalloc/jemalloc")
endif()
if(NOT DEFINED UNIMEMORY_STATISTICS OR NOT UNIMEMORY_STATISTICS MATCHES "^(ON|OFF)$")
    message(FATAL_ERROR "Set UNIMEMORY_STATISTICS explicitly to ON/OFF")
endif()
set(UNIMEMORY_BACKEND "${UNIMEMORY_BACKEND}" CACHE STRING "One selected backend")
set_property(CACHE UNIMEMORY_BACKEND PROPERTY STRINGS standard mimalloc jemalloc)
# ON fixes Global Basic and permits Heap Disabled/Basic; OFF fixes Global
# Disabled and rejects Heap Basic. Heap/Stack APIs and tracking code remain.
set(UNIMEMORY_STATISTICS "${UNIMEMORY_STATISTICS}" CACHE BOOL "Global Basic mode and Heap Basic support; OFF rejects Heap Basic")
if(UNIMEMORY_BACKEND STREQUAL "standard")
    set(UNIMEMORY_CONFIG_BACKEND 0)
elseif(UNIMEMORY_BACKEND STREQUAL "mimalloc")
    set(UNIMEMORY_CONFIG_BACKEND 1)
else()
    set(UNIMEMORY_CONFIG_BACKEND 2)
endif()
if(UNIMEMORY_STATISTICS STREQUAL "ON")
    set(UNIMEMORY_CONFIG_STATISTICS 1)
else()
    set(UNIMEMORY_CONFIG_STATISTICS 0)
endif()
set(UNIMEMORY_WITH_MIMALLOC OFF CACHE BOOL "Selected SDK only" FORCE)
set(UNIMEMORY_WITH_JEMALLOC OFF CACHE BOOL "Selected SDK only" FORCE)
if(UNIMEMORY_BACKEND STREQUAL "mimalloc")
    set(UNIMEMORY_WITH_MIMALLOC ON CACHE BOOL "Selected SDK only" FORCE)
elseif(UNIMEMORY_BACKEND STREQUAL "jemalloc")
    set(UNIMEMORY_WITH_JEMALLOC ON CACHE BOOL "Selected SDK only" FORCE)
endif()
option(UNIMEMORY_BUILD_BENCHMARKS "Build the opt-in allocation benchmark" OFF)
option(UNIMEMORY_BUILD_EXAMPLES "Build the public API examples" OFF)
option(UNIMEMORY_BUILD_DIAGNOSTICS "Build opt-in causal performance probes" OFF)
option(UNIMEMORY_AGGRESSIVE_INLINING "Use MSVC /Ob3 for Release consumers; OFF preserves their own inlining settings" ON)
if(UNIMEMORY_WITH_MIMALLOC)
    list(APPEND VCPKG_MANIFEST_FEATURES mimalloc)
    if(CMAKE_HOST_WIN32 AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg[.]cmake$")
        list(APPEND VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/triplets")
        if(NOT DEFINED VCPKG_TARGET_TRIPLET AND
           (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Windows") AND
           (NOT CMAKE_GENERATOR_PLATFORM OR CMAKE_GENERATOR_PLATFORM STREQUAL "x64") AND
           "$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64")
            set(VCPKG_TARGET_TRIPLET x64-windows-unimemory CACHE STRING "Explicit allocator backend profile")
        endif()
    endif()
endif()
if(UNIMEMORY_WITH_JEMALLOC AND
   (CMAKE_HOST_WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    list(APPEND VCPKG_MANIFEST_FEATURES jemalloc)
endif()

project(UniMemory VERSION 0.0.1 LANGUAGES CXX)
set(UNIMEMORY_SANITIZER "" CACHE STRING "Sanitizer: empty, address, or thread (GCC/Clang)")
set_property(CACHE UNIMEMORY_SANITIZER PROPERTY STRINGS "" address thread)
if(UNIMEMORY_SANITIZER)
    if(MSVC OR NOT UNIMEMORY_SANITIZER MATCHES "^(address|thread)$")
        message(FATAL_ERROR "UNIMEMORY_SANITIZER requires GCC/Clang and address or thread")
    endif()
    if(UNIMEMORY_SANITIZER STREQUAL "address")
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endif()
option(UNIMEMORY_COVERAGE "Instrument project tests for GCC coverage" OFF)
if(UNIMEMORY_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "UNIMEMORY_COVERAGE requires GCC")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()
set(_unimemory_test_default OFF)
if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    set(_unimemory_test_default ON)
endif()
option(UNIMEMORY_BUILD_TESTS "Build UniMemory's correctness tests" ${_unimemory_test_default})
unset(_unimemory_test_default)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

add_library(UniMemory INTERFACE)
add_library(UniMemory::UniMemory ALIAS UniMemory)
target_link_libraries(UniMemory INTERFACE Threads::Threads)

target_compile_features(UniMemory INTERFACE cxx_std_20)
target_include_directories(UniMemory INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(UniMemory PROPERTIES
    EXPORT_NAME UniMemory
    UNIMEMORY_VERSION "${PROJECT_VERSION}"
    INTERFACE_POSITION_INDEPENDENT_CODE ON)
set_property(TARGET UniMemory APPEND PROPERTY EXPORT_PROPERTIES UNIMEMORY_VERSION)
set_target_properties(UniMemory PROPERTIES
    UNIMEMORY_AGGRESSIVE_INLINING "${UNIMEMORY_AGGRESSIVE_INLINING}")
set_property(TARGET UniMemory APPEND PROPERTY EXPORT_PROPERTIES UNIMEMORY_AGGRESSIVE_INLINING)
target_compile_options(UniMemory INTERFACE
    "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>,$<BOOL:$<TARGET_PROPERTY:UniMemory::UniMemory,UNIMEMORY_AGGRESSIVE_INLINING>>>:/Ob3>")

# Resolve the target-wide precondition checking policy.
set(UNIMEMORY_CHECKS AUTO CACHE STRING "Precondition checks: AUTO/ON/OFF")
set_property(CACHE UNIMEMORY_CHECKS PROPERTY STRINGS AUTO ON OFF)
if(NOT UNIMEMORY_CHECKS MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "UNIMEMORY_CHECKS must be AUTO, ON, or OFF")
endif()
if(UNIMEMORY_CHECKS STREQUAL "ON")
    set(UNIMEMORY_CONFIG_CHECKS_POLICY 1)
    set(_unimemory_checks_value 1)
elseif(UNIMEMORY_CHECKS STREQUAL "OFF")
    set(UNIMEMORY_CONFIG_CHECKS_POLICY 2)
    set(_unimemory_checks_value 0)
else()
    set(UNIMEMORY_CONFIG_CHECKS_POLICY 0)
    set(_unimemory_checks_value "$<IF:$<CONFIG:Debug>,1,0>")
endif()

set(_unimemory_generated_include "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${_unimemory_generated_include}/unimem")
configure_file(include/unimem/config.h.in "${_unimemory_generated_include}/unimem/config.h" @ONLY)
target_include_directories(UniMemory BEFORE INTERFACE $<BUILD_INTERFACE:${_unimemory_generated_include}>)
target_compile_definitions(UniMemory INTERFACE
    UNIMEMORY_TARGET_CHECKS=${_unimemory_checks_value}
    UNIMEMORY_CHECKS=${_unimemory_checks_value})
set_target_properties(UniMemory PROPERTIES UNIMEMORY_CHECKS_POLICY "${UNIMEMORY_CHECKS}")
set_property(TARGET UniMemory APPEND PROPERTY EXPORT_PROPERTIES UNIMEMORY_CHECKS_POLICY)
set_target_properties(UniMemory PROPERTIES
    UNIMEMORY_BACKEND "${UNIMEMORY_BACKEND}"
    UNIMEMORY_STATISTICS "${UNIMEMORY_STATISTICS}")
set_property(TARGET UniMemory APPEND PROPERTY EXPORT_PROPERTIES UNIMEMORY_BACKEND UNIMEMORY_STATISTICS)
install(FILES "${_unimemory_generated_include}/unimem/config.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/unimem)


if(UNIMEMORY_WITH_MIMALLOC)
    find_package(mimalloc CONFIG REQUIRED)
    if(TARGET mimalloc)
        set(UNIMEMORY_MIMALLOC_TARGET mimalloc)
    elseif(TARGET mimalloc-static)
        set(UNIMEMORY_MIMALLOC_TARGET mimalloc-static)
    else()
        message(FATAL_ERROR "mimalloc package must export mimalloc or mimalloc-static")
    endif()
    get_target_property(UNIMEMORY_MIMALLOC_INCLUDE_HINTS
        ${UNIMEMORY_MIMALLOC_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    find_path(UNIMEMORY_MIMALLOC_INCLUDE_DIR mimalloc.h
        HINTS ${UNIMEMORY_MIMALLOC_INCLUDE_HINTS} REQUIRED)
    target_include_directories(UniMemory INTERFACE $<BUILD_INTERFACE:${UNIMEMORY_MIMALLOC_INCLUDE_DIR}>)
    target_link_libraries(UniMemory INTERFACE ${UNIMEMORY_MIMALLOC_TARGET})
    target_compile_definitions(UniMemory INTERFACE UNIMEMORY_WITH_MIMALLOC)
endif()

if(UNIMEMORY_WITH_JEMALLOC)
    include(cmake/UniMemoryJemalloc.cmake)
    if(NOT UNIMEMORY_JEMALLOC_FOUND)
        message(FATAL_ERROR "${UNIMEMORY_JEMALLOC_NOT_FOUND_MESSAGE}")
    endif()
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES "${UNIMEMORY_JEMALLOC_INCLUDE_DIR}")
    set(CMAKE_REQUIRED_LIBRARIES "${UNIMEMORY_JEMALLOC_RELEASE_LIBRARY}")
    check_cxx_source_compiles("#define JEMALLOC_NO_RENAME\n#include <jemalloc/jemalloc.h>\nint main(){void* p=je_mallocx(64,MALLOCX_ALIGN(64));je_dallocx(p,0);return p==nullptr;}"
        UNIMEMORY_JEMALLOC_EXPLICIT_API)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(NOT UNIMEMORY_JEMALLOC_EXPLICIT_API)
        message(FATAL_ERROR "jemalloc must export je_ functions; Unix needs --with-jemalloc-prefix=je_")
    endif()
    target_include_directories(UniMemory INTERFACE $<BUILD_INTERFACE:${UNIMEMORY_JEMALLOC_INCLUDE_DIR}>)
    target_link_libraries(UniMemory INTERFACE unimemory_jemalloc)
    target_compile_definitions(UniMemory INTERFACE UNIMEMORY_WITH_JEMALLOC)
endif()

if(UNIMEMORY_BUILD_TESTS AND (NOT DEFINED BUILD_TESTING OR BUILD_TESTING))
    enable_testing()
    if(UNIMEMORY_SANITIZER STREQUAL "address")
        add_executable(UniMemorySanitizerProbe tests/sanitizer_probe.cpp)
        add_test(NAME UniMemory.sanitizer.reject_undefined_behavior
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:UniMemorySanitizerProbe>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_sanitizer_failure.cmake")
    endif()
    find_package(Threads REQUIRED)
    set(_unimemory_header_tests
        tests/headers/common.cpp
        tests/headers/memory.cpp
        tests/headers/owned_block.cpp
        tests/headers/allocator.cpp
        tests/headers/smart_ptr.cpp
        tests/headers/standalone_allocator.cpp
        tests/headers/standalone_smart_ptr.cpp)
    add_executable(UniMemoryHeaderTests tests/headers/main.cpp ${_unimemory_header_tests})
    target_link_libraries(UniMemoryHeaderTests PRIVATE UniMemory::UniMemory)
    set_target_properties(UniMemoryHeaderTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME UniMemory.headers COMMAND UniMemoryHeaderTests)
    if(WIN32)
        add_executable(UniMemoryWindowsHeaders tests/headers/windows.cpp)
        target_link_libraries(UniMemoryWindowsHeaders PRIVATE UniMemory::UniMemory)
        add_test(NAME UniMemory.headers.windows COMMAND UniMemoryWindowsHeaders)
    endif()
    add_executable(UniMemoryGlobalSharedTests tests/global_shared_tests.cpp)
    target_link_libraries(UniMemoryGlobalSharedTests PRIVATE UniMemory::UniMemory)
    set_target_properties(UniMemoryGlobalSharedTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME UniMemory.global_shared.lifetime COMMAND UniMemoryGlobalSharedTests lifetime)
    if(UNIMEMORY_STATISTICS)
        add_test(NAME UniMemory.global_shared.stateless_shape COMMAND UniMemoryGlobalSharedTests stateless_shape)
    endif()
    add_executable(UniMemoryPmrLifetime
        tests/pmr_lifetime/main.cpp tests/pmr_lifetime/a.cpp tests/pmr_lifetime/b.cpp)
    target_link_libraries(UniMemoryPmrLifetime PRIVATE UniMemory::UniMemory)
    set_target_properties(UniMemoryPmrLifetime PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME UniMemory.pmr.cross_tu_static_lifetime COMMAND UniMemoryPmrLifetime)
    foreach(order RANGE 0 5)
        add_executable(UniMemoryHeaderOrder${order} tests/headers/include_order.cpp)
        target_compile_definitions(UniMemoryHeaderOrder${order} PRIVATE UNIMEMORY_HEADER_ORDER=${order})
        target_link_libraries(UniMemoryHeaderOrder${order} PRIVATE UniMemory::UniMemory)
        set_target_properties(UniMemoryHeaderOrder${order} PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME UniMemory.headers.order_${order} COMMAND UniMemoryHeaderOrder${order})
    endforeach()
    unset(_unimemory_header_tests)
    add_executable(UniMemoryJemallocNmFixture tests/jemalloc_nm_fixture.cpp)
    target_compile_features(UniMemoryJemallocNmFixture PRIVATE cxx_std_20)
    set_target_properties(UniMemoryJemallocNmFixture PROPERTIES CXX_EXTENSIONS OFF)
    foreach(scenario IN ITEMS missing_include missing_library missing_nm nm_error nm_unavailable
            release_replaces_new debug_replaces_new valid debug_fallback retry)
        set(_unimemory_lookup_modes optional required)
        if(scenario STREQUAL "retry")
            set(_unimemory_lookup_modes optional)
        elseif(UNIMEMORY_BACKEND STREQUAL "jemalloc" AND NOT scenario MATCHES "^(valid|debug_fallback)$")
            list(APPEND _unimemory_lookup_modes source)
        endif()
        foreach(mode IN LISTS _unimemory_lookup_modes)
            add_test(NAME "UniMemory.package.${scenario}_${mode}" COMMAND "${CMAKE_COMMAND}"
                "-DUNIMEMORY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DTEST_BINARY=${CMAKE_CURRENT_BINARY_DIR}/package-tests/${scenario}-${mode}"
                "-DTEST_GENERATOR=${CMAKE_GENERATOR}" "-DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                "-DSCENARIO=${scenario}" "-DLOOKUP_MODE=${mode}"
                "-DCOMPILED_BACKEND=${UNIMEMORY_BACKEND}" "-DCOMPILED_STATISTICS=${UNIMEMORY_STATISTICS}"
                "-DNM_FIXTURE=$<TARGET_FILE:UniMemoryJemallocNmFixture>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/package/run.cmake")
        endforeach()
    endforeach()
    unset(_unimemory_lookup_modes)
    add_executable(UniMemoryTypedStorage tests/typed_storage_tests.cpp)
    target_link_libraries(UniMemoryTypedStorage PRIVATE UniMemory::UniMemory)
    set_target_properties(UniMemoryTypedStorage PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(UniMemoryTypedStorage PRIVATE /W4 /permissive-)
    else()
        target_compile_options(UniMemoryTypedStorage PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors)
    endif()
    add_test(NAME UniMemory.typed_storage COMMAND UniMemoryTypedStorage)
    add_executable(UniMemoryTests tests/memory_tests.cpp)
    target_link_libraries(UniMemoryTests PRIVATE UniMemory::UniMemory Threads::Threads)
    target_compile_features(UniMemoryTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(UniMemoryTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(UniMemoryTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(UNIMEMORY_WITH_MIMALLOC)
        target_compile_definitions(UniMemoryTests PRIVATE UNIMEMORY_TEST_MIMALLOC)
        add_executable(UniMemoryMimallocPaths tests/mimalloc_paths.cpp)
        target_include_directories(UniMemoryMimallocPaths PRIVATE "${UNIMEMORY_MIMALLOC_INCLUDE_DIR}")
        target_link_libraries(UniMemoryMimallocPaths PRIVATE UniMemory::UniMemory ${UNIMEMORY_MIMALLOC_TARGET} Threads::Threads)
        foreach(path IN ITEMS default arena counters threads)
            if(path STREQUAL "counters" AND NOT UNIMEMORY_STATISTICS)
                continue()
            endif()
            add_test(NAME "UniMemory.mimalloc.paths_${path}" COMMAND UniMemoryMimallocPaths "${path}")
        endforeach()
        # Header-only SDK calls are emitted in the test's object files.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_executable(UniMemoryHeapResetFailure tests/heap_reset_failure.cpp)
            target_include_directories(UniMemoryHeapResetFailure PRIVATE "${UNIMEMORY_MIMALLOC_INCLUDE_DIR}")
            target_link_libraries(UniMemoryHeapResetFailure PRIVATE UniMemory::UniMemory ${UNIMEMORY_MIMALLOC_TARGET})
            target_link_options(UniMemoryHeapResetFailure PRIVATE "-Wl,--wrap=mi_heap_new")
            add_test(NAME UniMemory.mimalloc.reset_failure COMMAND UniMemoryHeapResetFailure)
        endif()
    endif()
    if(UNIMEMORY_WITH_JEMALLOC)
        target_compile_definitions(UniMemoryTests PRIVATE UNIMEMORY_TEST_JEMALLOC)
        add_executable(UniMemoryJemallocInitialization tests/jemalloc_initialization.cpp)
        target_link_libraries(UniMemoryJemallocInitialization PRIVATE UniMemory::UniMemory Threads::Threads)
        set_target_properties(UniMemoryJemallocInitialization PROPERTIES CXX_EXTENSIONS OFF)
        foreach(mode IN ITEMS shared global heap capabilities option mixed)
            add_test(NAME "UniMemory.jemalloc.initialization_${mode}" COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:UniMemoryJemallocInitialization>" "-DMODE=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/jemalloc_initialization.cmake")
            set_tests_properties("UniMemory.jemalloc.initialization_${mode}" PROPERTIES TIMEOUT 60)
        endforeach()
        add_test(NAME UniMemory.jemalloc.initialization_lifecycle
            COMMAND UniMemoryJemallocInitialization mixed 16 16)
        set_tests_properties(UniMemory.jemalloc.initialization_lifecycle PROPERTIES TIMEOUT 60)
    endif()
    add_test(NAME UniMemory.memory COMMAND UniMemoryTests)
    add_executable(UniMemoryUnifiedTests tests/unified_memory_tests.cpp)
    target_link_libraries(UniMemoryUnifiedTests PRIVATE UniMemory::UniMemory Threads::Threads)
    add_executable(UniMemoryInterfaceUsage tests/interface_usage_tests.cpp)
    target_link_libraries(UniMemoryInterfaceUsage PRIVATE UniMemory::UniMemory Threads::Threads)
    foreach(mode IN ITEMS owners objects allocators marks)
        add_test(NAME "UniMemory.stack.usage_${mode}"
            COMMAND UniMemoryInterfaceUsage "${UNIMEMORY_BACKEND}" stack "${mode}" 0 1024 disabled)
    endforeach()
    foreach(seed RANGE 0 31)
        add_test(NAME "UniMemory.stack.owner_trace_${seed}"
            COMMAND UniMemoryInterfaceUsage "${UNIMEMORY_BACKEND}" stack trace "${seed}" 1024 disabled)
    endforeach()
    add_executable(UniMemoryDeleterContracts tests/deleter_contract_tests.cpp)
    target_link_libraries(UniMemoryDeleterContracts PRIVATE UniMemory::UniMemory)
    # Probe Release failure behavior in every configuration, without assert dialogs.
    target_compile_definitions(UniMemoryDeleterContracts PRIVATE NDEBUG)
    add_test(NAME UniMemory.owner.contracts COMMAND UniMemoryDeleterContracts valid)
    # Only CHECKS-enabled configurations execute intentionally invalid owners.
    if(NOT UNIMEMORY_CHECKS STREQUAL "OFF")
        set(_unimemory_owner_configs)
        if(UNIMEMORY_CHECKS STREQUAL "AUTO")
            set(_unimemory_owner_configs CONFIGURATIONS Debug)
        endif()
        foreach(mode IN ITEMS unbound_object unbound_array zero_array_count)
            add_test(NAME "UniMemory.owner.reject_${mode}" COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:UniMemoryDeleterContracts>" "-DMODE=${mode}"
                "-DEXPECTED=93" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake"
                ${_unimemory_owner_configs})
        endforeach()
        unset(_unimemory_owner_configs)
    endif()
    add_test(NAME UniMemory.stack.unified_boundary COMMAND UniMemoryUnifiedTests stack boundary)
    foreach(index RANGE 0 239)
        add_test(NAME "UniMemory.stack.case_${index}" COMMAND UniMemoryUnifiedTests stack "${index}")
    endforeach()
    add_executable(UniMemoryConformance tests/conformance_tests.cpp)
    target_link_libraries(UniMemoryConformance PRIVATE UniMemory::UniMemory Threads::Threads)
    target_compile_features(UniMemoryConformance PRIVATE cxx_std_20)
    add_executable(UniMemoryHeapTests tests/heap_tests.cpp)
    target_link_libraries(UniMemoryHeapTests PRIVATE UniMemory::UniMemory Threads::Threads)
    add_test(NAME UniMemory.standard.arena_rejected
        COMMAND UniMemoryHeapTests standard rejected)
    add_executable(UniMemoryProductionTests tests/production_tests.cpp)
    target_link_libraries(UniMemoryProductionTests PRIVATE UniMemory::UniMemory Threads::Threads)
    add_executable(UniMemoryExtendedStress tests/extended_stress.cpp)
    target_link_libraries(UniMemoryExtendedStress PRIVATE UniMemory::UniMemory Threads::Threads)
    add_test(NAME UniMemory.scratch.production COMMAND UniMemoryProductionTests "${UNIMEMORY_BACKEND}" stack)
    # Clang TSan owns the global replacement-new symbols.
    if(NOT (UNIMEMORY_SANITIZER STREQUAL "thread" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
        add_executable(UniMemoryAllocationFailure tests/allocation_failure.cpp)
        target_link_libraries(UniMemoryAllocationFailure PRIVATE UniMemory::UniMemory)
        if(UNIMEMORY_STATISTICS)
            add_test(NAME UniMemory.global.initialization_retry
                COMMAND UniMemoryAllocationFailure initialization_retry)
        endif()
        if(UNIMEMORY_BACKEND STREQUAL "standard")
            if(UNIMEMORY_STATISTICS)
                set(_compiled_failure_mode basic)
            else()
                set(_compiled_failure_mode disabled)
            endif()
            add_test(NAME "UniMemory.standard.allocation_failure_${_compiled_failure_mode}"
                COMMAND UniMemoryAllocationFailure "${_compiled_failure_mode}")
        endif()
    elseif(UNIMEMORY_SANITIZER STREQUAL "thread" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Global-new fault injection runs in ordinary/ASan jobs: Clang TSan owns the replacement symbols")
    endif()
    set(UNIMEMORY_TEST_BACKENDS "${UNIMEMORY_BACKEND}")
    if(UNIMEMORY_STATISTICS)
        set(_compiled_global_mode basic)
    else()
        set(_compiled_global_mode disabled)
    endif()
    foreach(backend IN LISTS UNIMEMORY_TEST_BACKENDS)
        foreach(mode IN ITEMS churn asymmetric pressure)
            add_test(NAME "UniMemory.${backend}.extended_${mode}"
                COMMAND UniMemoryExtendedStress "${backend}" "${mode}")
            set_tests_properties("UniMemory.${backend}.extended_${mode}"
                PROPERTIES TIMEOUT 300 LABELS "stress;extended")
        endforeach()
        set(UNIMEMORY_USAGE_KINDS global)
        if(NOT backend STREQUAL "standard")
            list(APPEND UNIMEMORY_USAGE_KINDS heap)
        endif()
        foreach(statistics IN ITEMS disabled basic)
            if(statistics STREQUAL "basic" AND NOT UNIMEMORY_STATISTICS)
                continue()
            endif()
            if(statistics STREQUAL _compiled_global_mode)
            foreach(seed RANGE 0 2)
                add_test(NAME "UniMemory.${backend}.compiled_configuration_${statistics}_${seed}"
                    COMMAND UniMemoryInterfaceUsage "${backend}" global compiled-config "${seed}" 1024 "${statistics}")
            endforeach()
            endif()
            foreach(kind IN LISTS UNIMEMORY_USAGE_KINDS)
                if(kind STREQUAL "global" AND NOT statistics STREQUAL _compiled_global_mode)
                    continue()
                endif()
                foreach(mode IN ITEMS owners objects allocators handoff)
                    add_test(NAME "UniMemory.${backend}.${kind}.usage_${statistics}_${mode}"
                        COMMAND UniMemoryInterfaceUsage "${backend}" "${kind}" "${mode}" 0 1024 "${statistics}")
                    set_tests_properties("UniMemory.${backend}.${kind}.usage_${statistics}_${mode}" PROPERTIES TIMEOUT 120)
                endforeach()
                foreach(seed RANGE 0 31)
                    add_test(NAME "UniMemory.${backend}.${kind}.owner_trace_${statistics}_${seed}"
                        COMMAND UniMemoryInterfaceUsage "${backend}" "${kind}" trace "${seed}" 1024 "${statistics}")
                endforeach()
            endforeach()
        endforeach()
        foreach(mode IN ITEMS ${_compiled_global_mode})
            add_test(NAME "UniMemory.${backend}.global_${mode}" COMMAND UniMemoryUnifiedTests "${backend}" "${mode}")
        endforeach()
        foreach(seed RANGE 0 63)
            add_test(NAME "UniMemory.${backend}.trace_${seed}"
                COMMAND UniMemoryProductionTests "${backend}" trace "${seed}")
        endforeach()
        foreach(mode IN ITEMS objects containers boundaries handoff soak)
            add_test(NAME "UniMemory.${backend}.production_${mode}"
                COMMAND UniMemoryProductionTests "${backend}" "${mode}")
            set_tests_properties("UniMemory.${backend}.production_${mode}" PROPERTIES TIMEOUT 300)
        endforeach()
        set_tests_properties("UniMemory.${backend}.production_soak" PROPERTIES LABELS "stress")
        if(NOT backend STREQUAL "standard")
            foreach(mode IN ITEMS reset collect owns containers threads lifetime)
                add_test(NAME "UniMemory.${backend}.arena_${mode}"
                    COMMAND UniMemoryHeapTests "${backend}" "${mode}")
            endforeach()
        endif()
        foreach(mode IN ITEMS smoke boundary stress)
            add_test(NAME "UniMemory.${backend}.${mode}"
                COMMAND UniMemoryConformance "${mode}" "${backend}")
        endforeach()
        foreach(index RANGE 0 239)
            add_test(NAME "UniMemory.${backend}.case_${index}"
                COMMAND UniMemoryConformance matrix "${backend}" "${index}")
        endforeach()
    endforeach()
endif()

if(UNIMEMORY_BUILD_EXAMPLES)
    foreach(example IN ITEMS basic heap scratch)
        add_executable(UniMemoryExample_${example} "examples/${example}.cpp")
        target_link_libraries(UniMemoryExample_${example} PRIVATE UniMemory::UniMemory)
        if(UNIMEMORY_BUILD_TESTS AND (NOT DEFINED BUILD_TESTING OR BUILD_TESTING))
            add_test(NAME "UniMemory.example.${example}" COMMAND UniMemoryExample_${example})
        endif()
    endforeach()
endif()

if(UNIMEMORY_BUILD_BENCHMARKS)
    find_package(Threads REQUIRED)
    add_executable(UniMemoryBenchmark benchmarks/allocation_bench.cpp)
    target_link_libraries(UniMemoryBenchmark PRIVATE UniMemory::UniMemory Threads::Threads)
    target_compile_features(UniMemoryBenchmark PRIVATE cxx_std_20)
    add_executable(UniMemoryDeliveryBenchmark benchmarks/delivery_bench.cpp)
    target_link_libraries(UniMemoryDeliveryBenchmark PRIVATE UniMemory::UniMemory Threads::Threads ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(UniMemoryDeliveryBenchmark PRIVATE psapi)
    endif()
    if(UNIMEMORY_WITH_MIMALLOC)
        target_compile_definitions(UniMemoryDeliveryBenchmark PRIVATE UNIMEMORY_BENCH_MIMALLOC)
        target_include_directories(UniMemoryDeliveryBenchmark PRIVATE "${UNIMEMORY_MIMALLOC_INCLUDE_DIR}")
        target_link_libraries(UniMemoryDeliveryBenchmark PRIVATE ${UNIMEMORY_MIMALLOC_TARGET})
    endif()
    if(UNIMEMORY_WITH_JEMALLOC)
        target_compile_definitions(UniMemoryDeliveryBenchmark PRIVATE UNIMEMORY_BENCH_JEMALLOC)
        target_link_libraries(UniMemoryDeliveryBenchmark PRIVATE unimemory_jemalloc)
    endif()
    set(_unimemory_benchmark_sdk "")
    if(UNIMEMORY_WITH_MIMALLOC)
        set(_unimemory_benchmark_sdk ${UNIMEMORY_MIMALLOC_TARGET})
    elseif(UNIMEMORY_WITH_JEMALLOC)
        set(_unimemory_benchmark_sdk unimemory_jemalloc)
    endif()
    set(_unimemory_sdk_record "sdk_target=standard\nsdk_binary=\nsdk_linker_file=\n")
    if(_unimemory_benchmark_sdk)
        set(_unimemory_sdk_record "sdk_target=${_unimemory_benchmark_sdk}\nsdk_binary=$<TARGET_FILE:${_unimemory_benchmark_sdk}>\nsdk_linker_file=$<TARGET_LINKER_FILE:${_unimemory_benchmark_sdk}>\n")
    endif()
    set(_unimemory_runtime "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    if(MSVC AND NOT _unimemory_runtime)
        set(_unimemory_runtime "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/benchmark-build-$<CONFIG>.txt" CONTENT
        "core=header-only\nbackend=${UNIMEMORY_BACKEND}\nstatistics=${UNIMEMORY_STATISTICS}\nchecks=${UNIMEMORY_CHECKS}\nconfiguration=$<CONFIG>\ncompiler=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\nmsvc_runtime=${_unimemory_runtime}\nflags=${CMAKE_CXX_FLAGS}\nrelease_flags=${CMAKE_CXX_FLAGS_RELEASE}\ndebug_flags=${CMAKE_CXX_FLAGS_DEBUG}\nipo=$<TARGET_PROPERTY:UniMemoryDeliveryBenchmark,INTERPROCEDURAL_OPTIMIZATION>\ncompile_definitions=$<TARGET_PROPERTY:UniMemoryDeliveryBenchmark,COMPILE_DEFINITIONS>\ncompile_options=$<TARGET_PROPERTY:UniMemoryDeliveryBenchmark,COMPILE_OPTIONS>\n${_unimemory_sdk_record}")
    unset(_unimemory_benchmark_sdk)
    unset(_unimemory_sdk_record)
    unset(_unimemory_runtime)
endif()

if(UNIMEMORY_BUILD_DIAGNOSTICS)
    include(cmake/UniMemoryDiagnostics.cmake)
endif()

install(TARGETS UniMemory EXPORT UniMemoryTargets)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/UniMemory)
install(FILES
    include/unimem/common.h
    include/unimem/owned_block.h
    include/unimem/allocator.h
    include/unimem/smart_ptr.h
    include/unimem/memory.h
    include/unimem/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/unimem)
install(FILES include/unimem/detail/memory.inl
    include/unimem/detail/inline.h
    include/unimem/detail/backend.h
    include/unimem/detail/backend_system.inl
    include/unimem/detail/backend_mimalloc.inl
    include/unimem/detail/backend_jemalloc.inl
    include/unimem/detail/common.inl
    include/unimem/detail/memory_core.inl
    include/unimem/detail/global_storage.inl
    include/unimem/detail/allocation.inl
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/unimem/detail)
configure_package_config_file(cmake/UniMemoryConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/UniMemoryConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UniMemory)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/UniMemoryConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/UniMemoryConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/UniMemoryConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UniMemoryJemalloc.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UniMemory)
install(EXPORT UniMemoryTargets FILE UniMemoryTargets.cmake
    NAMESPACE UniMemory:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UniMemory)

if(UNIMEMORY_BUILD_TESTS AND (NOT DEFINED BUILD_TESTING OR BUILD_TESTING))
    add_executable(UniMemoryCompiledConfiguration tests/compiled-configuration.cpp)
    target_compile_features(UniMemoryCompiledConfiguration PRIVATE cxx_std_20)
    target_link_libraries(UniMemoryCompiledConfiguration PRIVATE UniMemory::UniMemory)
    add_test(NAME UniMemory.compiled.configuration COMMAND UniMemoryCompiledConfiguration)
endif()
