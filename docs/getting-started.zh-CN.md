# 构建与安装

[目录](README.zh-CN.md) · [English](getting-started.md) · **简体中文**

## 运行示例

按照 [README 构建步骤](../README.zh-CN.md#编译运行)编译，然后运行基本示例：

| 平台 | 程序 |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS | `build/UniMemory-release/UniMemoryExample_basic` |

输出：`3, 1024 bytes`。[更多示例](../examples/README.md)

## 接入项目

选择[使用源码或安装包](../README.zh-CN.md#cmake-接入)。链接 `UniMemory::UniMemory` 后，目标自动提供头文件路径和 C++20 设置。代码中包含 `<unimem/memory.h>` 即可。

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

将占位符换成安装目录的绝对路径。项目的 CMake 文件需使用[安装包目标](../README.zh-CN.md#使用安装包)。

## 构建选项

| 选项 | 默认 | 用途 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | 未由上层设置时 OFF | 构建动态库 |
| `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC` | OFF | 启用可选后端 |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | 构建可运行示例 |

只构建库时设置 `-DBUILD_TESTING=OFF`。

## 部署程序

CMake 还需能找到已启用的原生后端依赖。使用动态库时，将 UniMemory 和所需后端库随程序部署。见[后端配置](guides/backends.zh-CN.md#动态库部署)。

[对象所有权](guides/objects.zh-CN.md) · [标准容器](guides/containers.zh-CN.md) · [原始内存](guides/raw-memory.zh-CN.md)
