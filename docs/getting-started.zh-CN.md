# 构建与安装

[目录](README.zh-CN.md) · [English](getting-started.md) · **简体中文**

## 获取源码

下载 [dev 源码 ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip)，或克隆仓库：

```sh
git clone --branch dev https://github.com/dugan-dev/UniMemory.git
```

在包含 `CMakeLists.txt` 的源码目录执行构建命令。库版本由 `CMakeLists.txt` 声明；头文件、生成配置和 SDK 库须使用匹配的修订。

## 运行示例

按照 [README 构建步骤](../README.zh-CN.md#构建与安装)编译，然后运行基本示例：

| 平台 | 程序 |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS | `build/UniMemory-release/UniMemoryExample_basic` |

输出：`3, 1024 bytes`。[更多示例](../examples/README.md)

## 接入项目

按照[项目集成示例](../README.zh-CN.md#项目集成)。`UniMemory::UniMemory` 是纯头文件 `INTERFACE` 目标，传递头文件路径、生成配置、C++20 设置与所选 SDK 链接依赖。代码中包含 `<unimem/memory.h>` 即可。

## 安装库

编译后，在 UniMemory 仓库目录执行：

```sh
cmake --install build/UniMemory-release --config Release --prefix build/installed
```

在自己的项目目录中，将 CMake 指向该安装目录：

```sh
cmake -S . -B build "-DCMAKE_PREFIX_PATH=<UniMemory-install-prefix>"
cmake --build build --config Release
```

将占位符换成安装目录的绝对路径。项目的 CMake 文件需使用[安装包目标](../README.zh-CN.md#项目集成)。

README 的 `release` 预设使用 `build/UniMemory-release`。若通过 `cmake -B` 指定其他目录，运行示例及安装时也应使用该目录。

## 构建选项

| 选项 | 默认 | 用途 |
| --- | --- | --- |
| `UNIMEMORY_BACKEND` | 必填，无默认值 | `standard`、`mimalloc`、`jemalloc` 三选一 |
| `UNIMEMORY_STATISTICS` | 必填，无默认值 | `ON`：Global Basic，支持 Heap Basic；`OFF`：Global Disabled，拒绝 Heap Basic |
| `UNIMEMORY_CHECKS` | `AUTO` | Debug 开启检查，其他配置省略；可用 `ON`/`OFF` 覆盖策略 |
| `UNIMEMORY_AGGRESSIVE_INLINING` | ON | MSVC Release 使用 `/Ob3`；OFF 保留使用方自己的内联选项 |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | 构建可运行示例 |

仅安装头文件与包时设置 `-DUNIMEMORY_BUILD_TESTS=OFF`。UniMemory 不生成静态或动态库，`BUILD_SHARED_LIBS` 不用于选择其库形式。

源码集成时，在添加子目录前设置必填选项：

```cmake
set(UNIMEMORY_BACKEND standard CACHE STRING "UniMemory backend")
set(UNIMEMORY_STATISTICS OFF CACHE BOOL "UniMemory counters")
add_subdirectory(UniMemory)
```

安装包固定后端、统计和检查策略，并包含生成的 `<unimem/config.h>`。使用方通常直接继承目标配置；冲突的 CMake 请求或编译宏会被拒绝。不同配置使用独立的构建/安装目录，同一程序各翻译单元保持同一配置。见[迁移说明](migration.zh-CN.md)。

## 内联与优化

`UNIMEMORY_AGGRESSIVE_INLINING=ON` 默认向 MSVC 的 Release 使用方传递 `/Ob3`；Debug、其他编译器配置不增加该选项。它作用于链接接口目标的整个 C++ 翻译单元，包含其中的非 UniMemory 代码，不是只优化分配函数。代码大小和其他负载也可能变化。

源码构建或配置安装消费者时使用 `-DUNIMEMORY_AGGRESSIVE_INLINING=OFF` 可移除该目标传递的 `/Ob3`，自行选择内联选项。安装包以消费者目标属性控制该选择；它与固定的后端/统计/检查契约不同。项目不会自动开启全局 IPO/LTO。

[实测控制与限制](performance/header-only.zh-CN.md)记录 SDK/API 双方同选项的结果；`/Ob3` 仍是编译器启发式，不保证每个函数都内联或所有负载最快。

## 部署程序

使用安装包时，CMake 仍需找到所选原生 SDK。动态链接 SDK 时将其随程序部署；UniMemory 自身没有运行时库需要部署。见[后端配置](guides/backends.zh-CN.md#动态库部署)。

[对象所有权](guides/objects.zh-CN.md) · [标准容器](guides/containers.zh-CN.md) · [原始内存](guides/raw-memory.zh-CN.md)
