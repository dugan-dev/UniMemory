# 构建与安装

[目录](README.zh-CN.md) · [English](getting-started.md) · **简体中文**

## 获取源码

下载 [main 源码 ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/main.zip)，或克隆仓库：

```sh
git clone https://github.com/dugan-dev/UniMemory.git
```

在包含 `CMakeLists.txt` 的源码目录执行构建命令。库版本由 `CMakeLists.txt` 声明；头文件和库须来自同一修订。

## 运行示例

按照 [README 构建步骤](../README.zh-CN.md#构建与安装)编译，然后运行基本示例：

| 平台 | 程序 |
| --- | --- |
| Windows / Visual Studio | `build/UniMemory-release/Release/UniMemoryExample_basic.exe` |
| Linux / macOS | `build/UniMemory-release/UniMemoryExample_basic` |

输出：`3, 1024 bytes`。[更多示例](../examples/README.md)

## 接入项目

按照[项目集成示例](../README.zh-CN.md#项目集成)。链接 `UniMemory::UniMemory` 后，目标自动提供头文件路径和 C++20 设置。代码中包含 `<unimem/memory.h>` 即可。

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
| `BUILD_SHARED_LIBS` | 未由上层设置时 OFF | 构建动态库 |
| `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC` | OFF | 启用可选后端 |
| `UNIMEMORY_BUILD_EXAMPLES` | OFF | 构建可运行示例 |

只构建库时设置 `-DBUILD_TESTING=OFF`。

## 部署程序

CMake 还需能找到已启用的原生后端依赖。使用动态库时，将 UniMemory 和所需后端库随程序部署。见[后端配置](guides/backends.zh-CN.md#动态库部署)。

[对象所有权](guides/objects.zh-CN.md) · [标准容器](guides/containers.zh-CN.md) · [原始内存](guides/raw-memory.zh-CN.md)
