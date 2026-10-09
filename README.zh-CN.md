# UniMemory

[![CI](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml/badge.svg)](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml) [![Release](https://img.shields.io/github/v/release/dugan-dev/UniMemory)](https://github.com/dugan-dev/UniMemory/releases/latest) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard) [![CMake](https://img.shields.io/badge/CMake-3.25%2B-green.svg)](https://cmake.org/) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](docs/guides/backends.zh-CN.md)

C++20 统一内存库：通过同一接口使用 Standard、mimalloc、jemalloc，管理对象、标准容器和字节缓冲区。

[English](README.md) · **简体中文**

## 特点

- 对象/数组构造、异常清理、独占与共享所有权。
- Allocator 和 PMR 标准容器适配。
- Global 分配、独立 Heap、固定缓冲区 Stack。
- 对齐、拥有型 Buffer、重分配与可选统计。
- 复用成熟显式后端，不替换全局 `new`/`delete`。

## 快速开始

```cpp
#include <unimem/memory.h>
#include <vector>

struct Point { float x, y; };

int main() {
    auto& memory = unimem::Memory::global();
    auto point = memory.make_unique<Point>(1.0f, 2.0f);
    std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
    values.push_back(20);
    auto bytes = memory.make_block(1024, 64);
    return point->x == 1.0f && values[0] == 20 && bytes.size() == 1024 ? 0 : 1;
}
```

Memory 必须活过其对象、容器和 weak_ptr 控制块；Stack 缓冲区必须活过 Memory。先销毁受影响对象，再 reset/rewind，这些操作不会调用对象析构函数。Global/Heap 允许并发分配与释放；回收须独占，Stack 为单线程。见[生命周期与兼容性](docs/compatibility.zh-CN.md)。

## CMake 接入

源码接入：

```cmake
add_subdirectory(UniMemory)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

安装包接入：

```cmake
find_package(UniMemory 0.0.1 CONFIG REQUIRED)
target_link_libraries(app PRIVATE UniMemory::UniMemory)
```

目标提供头文件和 C++20 配置。可选 `find_package(UniMemory QUIET CONFIG)` 在编译所需后端依赖缺失时返回 `UniMemory_FOUND=FALSE`。保持编译器和运行库配置兼容。0.x 要求头文件与库匹配，并重新编译消费者；已发布标签保留历史快照，main 可以包含后续修订。

## 后端与平台

| 能力 | Standard | mimalloc | jemalloc |
|---|:---:|:---:|:---:|
| 对象、容器、块 | 支持 | 支持 | 支持 |
| 请求统计 | 支持 | 支持 | 支持 |
| 独立 Heap | 不支持 | 支持 | 支持 |
| 附加依赖 | 无 | 可选 | 可选 |

Standard 始终可用。构建时启用 `UNIMEMORY_WITH_MIMALLOC` / `UNIMEMORY_WITH_JEMALLOC`，运行时用 `available()`、`capabilities()` 检查。CI 配置 Windows、Linux、macOS；具体修订的验证范围以测试记录为准。私有前缀共享后端需要正确配置动态库搜索路径。[后端配置](docs/guides/backends.zh-CN.md)

## 编译与测试

需要 CMake 3.25+、C++20 和兼容编译器。

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/release --parallel 4
ctest --test-dir build/release --output-on-failure
cmake --install build/release --prefix build/installed
```

Visual Studio 增加 `--config Release` / `-C Release`。发布前运行 `python tools/check-docs.py`。测试覆盖独立头文件、类型化存储复用、可选包发现、生命周期、边界、异常、并发及安装消费者。[测试记录](docs/testing.zh-CN.md) · [C++20 存储论证](docs/typed-storage-lifetime.md)

## 性能

统一接口、所有权和启用的计数器都有实际成本。速度与内存取决于负载、后端和平台，不宣称普遍最快或内存最低。已有测量和复现数据保留在[性能报告](docs/performance.zh-CN.md)。

## 文档

- [构建与安装](docs/getting-started.zh-CN.md) · [可运行示例](examples/README.md)
- [对象与数组](docs/guides/objects.zh-CN.md) · [容器](docs/guides/containers.zh-CN.md) · [字节缓冲区](docs/guides/raw-memory.zh-CN.md)
- [Heap](docs/guides/heap.zh-CN.md) · [Stack](docs/guides/stack.zh-CN.md) · [统计](docs/guides/statistics.zh-CN.md)
- [API 参考](docs/api-reference.zh-CN.md) · [完整文档](docs/README.zh-CN.md)
- [贡献流程](CONTRIBUTING.md) · [安全报告](SECURITY.md)

## 许可证

[MIT](LICENSE)，可选分配器保留各自许可证。
