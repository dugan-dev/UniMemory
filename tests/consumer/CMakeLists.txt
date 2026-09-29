cmake_minimum_required(VERSION 3.25)
project(UniMemoryInstalledConsumer LANGUAGES CXX)

find_package(UniMemory 0.0.1 EXACT CONFIG REQUIRED)
add_executable(UniMemoryInstalledConsumer main.cpp
    ../headers/common.cpp
    ../headers/memory.cpp
    ../headers/owned_block.cpp
    ../headers/allocator.cpp
    ../headers/smart_ptr.cpp)
target_link_libraries(UniMemoryInstalledConsumer PRIVATE UniMemory::UniMemory)
target_compile_features(UniMemoryInstalledConsumer PRIVATE cxx_std_20)
enable_testing()
add_test(NAME UniMemory.installed_consumer COMMAND UniMemoryInstalledConsumer)
