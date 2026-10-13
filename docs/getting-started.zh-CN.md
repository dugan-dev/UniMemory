# 构建和集成

[首页](README.zh-CN.md) · [English](getting-started.md)

## 获取源码

需要 CMake 3.25 或更高版本及支持 C++20 的编译器。下载并解压 [dev 源码 ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip)，或执行：

```sh
git clone --branch dev https://github.com/dugan-dev/UniMemory.git
cd UniMemory
```

以下命令在包含 `CMakeLists.txt` 的目录运行。Standard 后端不需要分配器 SDK，也不需要下载依赖。头文件、生成配置和可选 SDK 库应来自相互匹配的版本。

## 构建和运行

`release` 预设选择 `standard`、统计 `OFF`、检查 `AUTO`。显式启用示例：

```sh
cmake --preset release -DUNIMEMORY_BUILD_TESTS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
```

Windows 使用 Visual Studio 时，在 PowerShell 运行：

```powershell
./build/UniMemory-release/Release/UniMemoryExample_basic.exe
./build/UniMemory-release/Release/UniMemoryExample_heap.exe
./build/UniMemory-release/Release/UniMemoryExample_scratch.exe
```

Linux/macOS 使用单配置生成器时：

```sh
./build/UniMemory-release/UniMemoryExample_basic
./build/UniMemory-release/UniMemoryExample_heap
./build/UniMemory-release/UniMemoryExample_scratch
```

依次输出 `3, 1024 bytes`、`Heap reset succeeded`、`0 bytes used after rewind`。Debug 使用 `debug` 配置和构建预设，并传入相同选项；Visual Studio 的程序位于 `build/UniMemory-debug/Debug`。

不使用预设时，可执行等效的 Release 构建：

```sh
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_TESTS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/local --config Release --parallel
```

## 将源码接入项目

把仓库放在 `MyApp/UniMemory`，创建 `MyApp/CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)
set(UNIMEMORY_BACKEND standard CACHE STRING "UniMemory backend")
set(UNIMEMORY_STATISTICS OFF CACHE BOOL "UniMemory counters")
set(UNIMEMORY_BUILD_TESTS OFF CACHE BOOL "UniMemory tests")
add_subdirectory(UniMemory)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

将下面任一完整程序保存为 `MyApp/main.cpp`，在 `MyApp` 中运行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Visual Studio 运行 `./build/Release/my_app.exe`；单配置生成器运行 `./build/my_app`。`UniMemory::UniMemory` 提供 C++20、头文件路径、生成配置、线程库和所选 SDK 的链接依赖。UniMemory 本身仅由头文件组成，应用包含 `<unimem/all.h>`。

## 安装并使用包

在 UniMemory 仓库中执行：

```sh
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_TESTS=OFF
cmake --build build/package --config Release
cmake --install build/package --config Release --prefix build/installed
```

另建应用目录，包含 `main.cpp` 和以下 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp LANGUAGES CXX)
find_package(UniMemory CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

在应用目录运行，将占位符替换为绝对安装路径：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<absolute-UniMemory-path>/build/installed"
cmake --build build --config Release
```

安装内容包括头文件、生成配置和 CMake 包文件，不产生 UniMemory 静态库或动态库，`BUILD_SHARED_LIBS` 不改变这一点。使用可选 mimalloc/jemalloc SDK 时，消费者也必须能找到它；在 `CMAKE_PREFIX_PATH` 中用分号分隔 UniMemory 与 SDK 的安装前缀。动态链接的 SDK 需随应用部署。

## 示例

以下程序均可用于上述 Standard 配置。所有上下文的运行时统计默认为 `Disabled`。

### Global：对象、拥有的字节块与 PMR 容器

```cpp
#include <unimem/all.h>

#include <iostream>
#include <memory_resource>
#include <vector>

struct Point {
  float x;
  float y;
};

int main() {
  unimem::Global memory;
  Point* point = memory.create<Point>(1.0f, 2.0f);
  const Point position = *point;
  memory.destroy(point);

  unimem::OwnedBlock block = memory.make_block(1024, 64);
  std::pmr::vector<Point> points(memory.resource());
  points.push_back(position);
  std::cout << points.front().x + points.front().y << ", " << block.size() << " bytes\n";
  return 0;
}
```

### Heap：先释放所有者，再重置分组

```cpp
#include <unimem/all.h>

#include <iostream>

int main() {
  unimem::Heap heap;
  {
    unimem::Unique<int> value = heap.make_unique<int>(42);
    if (!heap.owns(value.get()) || *value != 42) {
      return 1;
    }
    heap.collect();
  }
  heap.reset();
  std::cout << "Heap reset succeeded\n";
  return 0;
}
```

`collect()` 保持存活分配有效。`reset()` 释放分组中的分配，但不调用对象析构函数；先销毁对象并释放所有者和容器。

### Stack：借用缓冲区并回退检查点

```cpp
#include <unimem/all.h>

#include <cstddef>
#include <iostream>

int main() {
  alignas(64) std::byte buffer[4096];
  unimem::Stack scratch(buffer, sizeof buffer);
  unimem::Stack::Mark checkpoint = scratch.mark();
  std::byte* bytes = static_cast<std::byte*>(scratch.allocate(128, 64));
  bytes[0] = std::byte{42};
  scratch.rewind(checkpoint);
  std::cout << scratch.used() << " bytes used after rewind\n";
  return scratch.used() == 0 ? 0 : 1;
}
```

缓冲区必须比 Stack 及其使用者活得更久。`rewind()` 使标记之后的分配失效，不调用析构函数；回退前先销毁受影响对象并释放所有者。Memory 上下文必须比其对象、所有者、分配器和 PMR 容器活得更久。

## 构建选项

| 选项 | 默认值 | 含义 |
| --- | --- | --- |
| `UNIMEMORY_BACKEND` | 必填 | 只能选择 `standard`、`mimalloc` 或 `jemalloc` |
| `UNIMEMORY_STATISTICS` | 必填 | `ON` 允许运行时 `Basic`；`OFF` 拒绝它；两者运行时均默认 `Disabled` |
| `UNIMEMORY_CHECKS` | `AUTO` | 仅 Debug 启用检查；`ON`/`OFF` 显式覆盖 |
| `UNIMEMORY_AGGRESSIVE_INLINING` | `ON` | 向 MSVC Release 消费者添加 `/Ob3` |
| `UNIMEMORY_BUILD_EXAMPLES` | `OFF` | 构建 basic、heap、scratch 示例 |
| `UNIMEMORY_BUILD_TESTS` | 顶层 CTest 设置 | 仅安装头文件和包时设为 `OFF` |

整个程序统一使用一套后端、统计和检查策略；不同配置使用独立的构建和安装目录。安装包保留这些设置，并拒绝消费者的冲突请求。不要只复制源码头文件，也不要覆盖生成配置宏。

需要统计时，以 `-DUNIMEMORY_STATISTICS=ON` 构建，并在构造时显式选择 `unimem::StatisticsMode::Basic`，例如 `unimem::Heap heap(unimem::StatisticsMode::Basic);`。

需要保留自己的 MSVC Release 内联选项时，设置 `-DUNIMEMORY_AGGRESSIVE_INLINING=OFF`；安装包消费者也可设置。

示例遵循仓库 `.clang-format`：两空格缩进、同一行大括号、指针星号靠左、150 列上限。若已安装 clang-format，可从仓库运行 `clang-format -i main.cpp`，让它使用仓库配置。
