# 平台与后端

[首页](../README.zh-CN.md) · [English](backends.md)

## 平台支持

UniMemory 需要 C++20 和 CMake 3.25 或更高版本。项目记录的平台覆盖如下：

| 平台与编译器 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| Windows x64 / MSVC | 已验证 | 已验证 | 已验证 |
| Windows ARM64 / MSVC、clang-cl | 已验证 | 未验证 | 未验证 |
| Linux x64 / GCC | 已验证 | 已验证 | 已验证，使用 `je_` 前缀 |
| Linux ARM64 / GCC、Clang | 已验证 | 未验证 | 未验证 |
| macOS Apple Silicon / AppleClang | 已验证 | 已验证 | 已验证，使用 `je_` 前缀 |
| macOS Intel / AppleClang | 已验证 | 未验证 | 未验证 |
| Android / iOS | 未进行设备测试 | 未进行设备测试 | 未进行设备测试 |

覆盖情况不保证所有操作系统版本和架构。交叉编译还需要面向目标平台的编译器、工具链、分配器 SDK 和部署库；不要把主机 SDK 库用于目标构建。

## 选择构建配置

必须同时设置 `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` 和 `UNIMEMORY_STATISTICS=ON|OFF`。CMake 不提供隐式后端或统计设置。仓库的 `release` 和 `debug` 预设显式选择 `standard` 与 `OFF`。

| 后端 | 项目使用的 SDK 版本 | 要求 |
| --- | --- | --- |
| `standard` | C++20 标准库 | 进程全局 `new/delete`，无需分配器 SDK |
| `mimalloc` | 3.4.3 | v3.4.3+ 头文件与匹配的库；更新版本需单独验证 |
| `jemalloc` | 5.3.1 | 匹配的头文件和库，导出 `je_` 函数且不替换全局 C++ `new/delete` |

vcpkg manifest 保持 baseline `2f1d605400c8727cc00c15797aba796c88ccd523`；Windows CI 使用 vcpkg 提交 `2273a28f34ce5aac8be50b3b6b44da7fc1722e06`。复用其他已安装包不会自动将它们调整为这些固定版本。

`Global`、`Heap`、`Stack` 直接构造，共享 `Memory` 接口。Global 实例共用原生分配器，但各有统计和 PMR resource 身份。所有后端都支持独立分配组 Heap。Stack 从调用者提供的固定缓冲区分配。整个程序在构建时选定后端，不通过公共运行时选择器切换。

## Standard：无需 SDK

在 UniMemory 仓库中执行：

```sh
cmake -S . -B build/standard -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/standard --config Release --parallel
ctest --test-dir build/standard -C Release --output-on-failure
```

这些命令仅使用本地工具。仅构建安装包时添加 `-DUNIMEMORY_BUILD_TESTS=OFF`。

## Windows：使用 vcpkg SDK

使用已有 vcpkg 仓库，将 `VCPKG_ROOT` 设为它的绝对路径。在 UniMemory 仓库中通过 PowerShell 执行：

```powershell
$env:VCPKG_ROOT = 'C:/tools/vcpkg'
cmake -S . -B build/mimalloc "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/mimalloc --config Release --parallel
```

manifest 安装可能下载依赖。新的 mimalloc Windows x64 构建默认使用 `x64-windows-unimemory`，禁用 CRT 重定向。显式指定的 triplet 保持不变；确认它也禁用了 mimalloc 的全局分配替换。选择 jemalloc 时使用 `-DUNIMEMORY_BACKEND=jemalloc`，并采用独立目录，例如 `build/jemalloc`。

复用已有安装且不改变包集合：

```powershell
cmake -S . -B build/mimalloc-existing `
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_INSTALLED_DIR=<installed-root>" "-DVCPKG_TARGET_TRIPLET=<installed-triplet>" `
    -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF `
    -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/mimalloc-existing --config Release --parallel
```

将两个占位符替换为已有安装的值。root 包含各 triplet 目录，而不是 triplet 目录本身。所选 SDK 必须已经安装，头文件与库匹配，且仅通过显式 API 分配。改变工具链或 triplet 时使用新构建目录。对共享安装根目录运行另一份 manifest，可能删除其中未声明的包；上述复用命令避开了安装步骤。

## Linux/macOS：配置 SDK

通过安装前缀复用已有兼容 SDK：

```sh
cmake -S . -B build/mimalloc -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<allocator-install-prefix>" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build build/mimalloc --parallel
ctest --test-dir build/mimalloc -C Release --output-on-failure
```

jemalloc 改用 `jemalloc` 后端和独立构建目录。系统默认 jemalloc 包可能缺少所需的显式 API。在 Unix 上，UniMemory 还需要目标工具链的 `nm`，以检查库是否替换了全局 C++ 分配。

如果需要从源码构建 SDK，从官方发布源获取 mimalloc 3.4.3 和 jemalloc 5.3.1，分别解压到 `build/mimalloc` 和 `build/jemalloc-5.3.1`。在 UniMemory 仓库中执行：

```sh
cmake -S build/mimalloc -B build/mi -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DMI_BUILD_TESTS=OFF "-DCMAKE_INSTALL_PREFIX=$PWD/build/prefix"
cmake --build build/mi --parallel
cmake --install build/mi

(cd build/jemalloc-5.3.1 && ./configure --with-jemalloc-prefix=je_ --disable-cxx --prefix="$PWD/../prefix" && make -j4 && make install)

cmake -S . -B build/jemalloc -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$PWD/build/prefix" -DUNIMEMORY_BACKEND=jemalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/jemalloc --parallel
```

SDK 源码发布：[mimalloc 3.4.3](https://github.com/microsoft/mimalloc/releases/tag/v3.4.3)、[jemalloc 5.3.1](https://github.com/jemalloc/jemalloc/releases/tag/5.3.1)。jemalloc 需要可用的 Unix `make` 工具链；其[官方构建选项](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md)说明了平台配置。

mimalloc 设置 `MI_OVERRIDE=OFF`；Windows 还需 `MI_WIN_REDIRECT=OFF`。jemalloc 必须同时使用 `--with-jemalloc-prefix=je_` 与 `--disable-cxx`：只有前缀仍可能替换全局 C++ `new/delete`，UniMemory 会拒绝这样的 Unix 库。不要将显式 UniMemory 分配与全局替换的 SDK 构建混用。

## 运行时配置

应用包含 `<unimem/all.h>`，直接构造上下文：

```cpp
#include <unimem/all.h>

#include <cstddef>

int main() {
  unimem::Global global;
  unimem::Heap heap;
  alignas(64) std::byte buffer[4096];
  unimem::Stack stack(buffer, sizeof buffer);
  auto value = heap.make_unique<int>(42);
  return *value == 42 ? 0 : 1;
}
```

三个类型的统计均默认 `Disabled`。`UNIMEMORY_STATISTICS=ON` 允许构造时显式选择 `StatisticsMode::Basic`，不会自动启用统计。`OFF` 构建拒绝 Basic。所有翻译单元、插件和安装包消费者必须统一后端、统计和检查策略。

原生分配器的环境设置应在进程启动前完成。若 mimalloc DLL 构建时启用了 CRT 重定向，需要显式分配行为时设置 `MIMALLOC_DISABLE_REDIRECT=1`；优先使用构建时已禁用重定向的 SDK。UniMemory 不在运行时修改进程环境变量。SDK 原生计数器与调优独立于 UniMemory 的请求字节统计。

## 安装和部署 Release 应用

构建后，以同一目录和配置安装 UniMemory：

```sh
cmake --install build/mimalloc --config Release --prefix build/installed
```

在独立应用目录中，将上面的完整运行时示例保存为 `main.cpp`，创建以下 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(BackendApp LANGUAGES CXX)
find_package(UniMemory CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

在应用目录执行，将占位符替换为绝对安装路径，确保配置时同时找到 UniMemory 和 SDK：

```sh
cmake -S . -B build/app -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<UniMemory-install-prefix>;<SDK-install-prefix>"
cmake --build build/app --config Release
```

Windows 消费者配置时传入同一 vcpkg 工具链和 triplet，或添加包含 SDK CMake 包的已有 triplet 前缀。安装包保留后端、统计和检查策略，拒绝冲突。应用只需包含 `<unimem/all.h>`，无需为使用 UniMemory 而包含 SDK 头文件。

| 平台 | 动态链接 SDK 的部署 |
| --- | --- |
| Windows | 把 Release SDK DLL 放在可执行文件旁或 DLL 搜索路径中；vcpkg Release DLL 位于 `<installed-root>/<triplet>/bin` |
| Linux | 部署 SDK `.so` 文件，并配置应用的运行时库搜索路径 |
| macOS | 部署 SDK dylib，确保 install name 与 rpath 匹配 |

自定义 Unix 布局时，将应用的 `CMAKE_INSTALL_RPATH` 设置为部署后的 SDK 位置。UniMemory 自身没有需要部署的运行时库。静态链接 SDK 不需要额外的 SDK DLL 或共享库。目标架构、编译器和 CRT 配置必须匹配。jemalloc 的 Release 从 `lib` 查找，Debug 优先从 `debug/lib` 查找；没有 Debug 库时回退到发现的 Release 库，需确保该回退与工具链兼容。
