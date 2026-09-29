# 快速开始

[目录](README.zh-CN.md) · [English](getting-started.md) · **简体中文**

## 编译运行

需要 C++20 编译器、CMake 3.25+。Standard 无额外分配器依赖。

```sh
git clone https://github.com/dugan-dev/UniMemory.git
cd UniMemory
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

| 平台 | 示例程序 |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS，单配置 | `build/UniMemory-release/UniMemoryExample_basic` |

运行输出 `3, 1024 bytes`。Visual Studio 用 preset 选择 Release；它提示 `CMAKE_BUILD_TYPE` 未使用不影响构建。

## 接入源码

把仓库放入项目的 `UniMemory/`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)
add_subdirectory(UniMemory)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

以 [basic.cpp](../examples/basic.cpp) 作为 `main.cpp`。子项目默认不构建 UniMemory 测试或示例；目标自动传递 C++20 和头文件目录。

## 安装并使用

```sh
cmake --install build/UniMemory-release --config Release --prefix build/installed
cmake -S tests/consumer -B build/consumer -DCMAKE_PREFIX_PATH="<absolute-path-to-build/installed>"
cmake --build build/consumer --config Release
ctest --test-dir build/consumer -C Release --output-on-failure
```

将占位符换成安装目录的绝对路径。自己的项目使用：

```cmake
find_package(UniMemory 0.0.1 CONFIG REQUIRED)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

启用原生 Backend 后，配置消费者也需能找到对应依赖；动态库另需部署运行库。[Backend 配置 →](guides/backends.zh-CN.md)

## 构建开关

| 选项 | 默认 | 用途 |
| --- | --- | --- |
| `UNIMEMORY_BUILD_TESTS` | 独立项目 ON，子项目 OFF | 本库测试 |
| `BUILD_TESTING` | 独立项目 ON | OFF 也关闭本库测试 |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | 三个可运行示例 |
| `UNIMEMORY_BUILD_BENCHMARKS` | OFF | 性能基准 |
| `BUILD_SHARED_LIBS` | 未由上层设置时 OFF | 动态库 |
| `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC` | OFF | 可选 Backend |

只构建库：`cmake -S . -B build/library -DBUILD_TESTING=OFF`。

下一步：[Object](guides/objects.zh-CN.md) → [Container](guides/containers.zh-CN.md) → [Block](guides/raw-memory.zh-CN.md)
