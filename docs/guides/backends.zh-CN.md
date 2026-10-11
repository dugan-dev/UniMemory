# 平台与后端

[目录](../README.zh-CN.md) · [English](backends.md) · **简体中文**

## 平台验证状态

| 平台 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| Windows x64 / MSVC | 已验证 | 已验证 | 已验证 |
| Linux x64 / GCC | 已验证 | 已验证 | 已验证，前缀构建 |
| macOS / Apple Clang | GitHub CI | GitHub CI | GitHub CI，前缀构建 |
| Android / iOS | 尚未设备验证 | 尚未设备验证 | 尚未设备验证 |

已测版本：mimalloc 3.4.3、jemalloc 5.3.1；更新版本需另行验证。[测试结果](../testing.zh-CN.md)

## 选择与构建

配置时必须显式设置 `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` 与 `UNIMEMORY_STATISTICS=ON|OFF`。每次构建仅选择一个分配后端，`Memory` 仍是非模板类。`Memory::global()` 返回该配置的共享 Global；显式 `global(backend)`、`heap(backend)` 必须匹配 `Memory::selected_backend`，否则抛 `std::invalid_argument`。`available(backend)` 仅对所选后端为 true，未选择的 Standard 也不能作为备用后端。

Heap 按支持情况创建独立分配组。统计 ON 固定 Global Basic，Heap 可选 Disabled/Basic；OFF 固定 Global Disabled，拒绝 Heap Basic。Stack 没有后端或统计。[统计说明](statistics.zh-CN.md)

| 后端 | CMake 配置 | 依赖要求 |
| --- | --- | --- |
| Standard | `UNIMEMORY_BACKEND=standard` | C++20，使用进程的标准分配路径 |
| mimalloc | `UNIMEMORY_BACKEND=mimalloc` | 3.4.3+ 头文件和匹配库 |
| jemalloc | `UNIMEMORY_BACKEND=jemalloc` | 5.3.1 已验证，显式 `je_` 导出 |

### Windows

将 `VCPKG_ROOT` 设为现有 vcpkg 路径，在 PowerShell 执行：

```powershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --config Release
```

仓库 manifest 固定依赖版本。新建 mimalloc x64 构建默认使用 `x64-windows-unimemory`，关闭 mimalloc CRT 重定向；显式指定的 triplet 保持原配置。下载 SDK 依赖需要网络。下方命令选择 mimalloc；选择 jemalloc 时在独立构建目录将后端值改为 `jemalloc`。

若复用现有 vcpkg 安装且不改变包集合，在新的构建目录中指定安装根目录和实际 triplet：

```powershell
cmake -S . -B build/backends-existing `
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_INSTALLED_DIR=<installed-root>" "-DVCPKG_TARGET_TRIPLET=<installed-triplet>" `
    -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF `
    -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
```

安装根目录包含各 triplet 子目录，不是 triplet 目录本身。所选后端包必须已经存在，且使用兼容的显式调用构建。不要让不同 manifest 共用同一安装根目录执行安装：vcpkg 可能删除该 manifest 不需要的包。Classic 模式复用所提供的包，不会追溯应用本仓库 manifest 的版本固定。

### Linux / macOS

先安装匹配后端，将占位符换成安装目录：

```sh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --parallel
```

jemalloc 必须以 `--with-jemalloc-prefix=je_ --disable-cxx` 构建，默认系统包不能直接使用。仅设置前缀仍可能替换全局 `new/delete`，UniMemory 拒绝这类库。[官方构建选项](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md)

手动构建 mimalloc 时使用 `MI_OVERRIDE=OFF`；Windows 另设 `MI_WIN_REDIRECT=OFF`。应用代码仅包含 `<unimem/memory.h>`。

## 动态库部署

| 平台 | 部署方式 |
| --- | --- |
| Windows | 所选 SDK DLL 放在程序目录或 DLL 搜索路径 |
| Linux | 部署所选 SDK `.so`，配置应用的运行时库搜索路径 |
| macOS | 按 dylib 的 install name/rpath 部署 |

UniMemory 是纯头文件库，只需部署动态链接的 SDK 二进制。自定义 Unix 布局使用应用的 `CMAKE_INSTALL_RPATH`；接口目标仍传递所选 SDK 链接，配置使用方时 CMake 也须找到该 SDK。使用匹配的编译器与运行库配置；jemalloc 的 Release/Debug 库分别取自 `lib`/`debug/lib`，仅有一份时共用。

mimalloc DLL 若启用了 CRT 重定向，可在程序启动前设置 `MIMALLOC_DISABLE_REDIRECT=1`，避免接管整个进程。UniMemory 不在运行中修改环境变量。

[能力对照](../allocator-capabilities.zh-CN.md) · [mimalloc](../backends/mimalloc.zh-CN.md) · [jemalloc](../backends/jemalloc.zh-CN.md)
