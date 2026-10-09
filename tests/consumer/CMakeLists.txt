cmake_minimum_required(VERSION 3.25)
project(UniMemoryInstalledConsumer LANGUAGES CXX)

find_package(UniMemory 0.0.1 EXACT CONFIG REQUIRED)
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
add_test(NAME UniMemory.installed_consumer COMMAND UniMemoryInstalledConsumer)
foreach(order RANGE 0 5)
    add_executable(UniMemoryInstalledHeaderOrder${order} ../headers/include_order.cpp)
    target_compile_definitions(UniMemoryInstalledHeaderOrder${order} PRIVATE UNIMEMORY_HEADER_ORDER=${order})
    target_link_libraries(UniMemoryInstalledHeaderOrder${order} PRIVATE UniMemory::UniMemory)
    set_target_properties(UniMemoryInstalledHeaderOrder${order} PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME UniMemory.installed_header_order_${order} COMMAND UniMemoryInstalledHeaderOrder${order})
endforeach()
