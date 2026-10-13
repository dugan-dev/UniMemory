# 平台与后端

[目录](../README.zh-CN.md) · [English](backends.md) · **简体中文**

## 平台验证状态

| 平台 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| Windows x64 / MSVC | 已验证 | 已验证 | 已验证 |
| Windows ARM64 / MSVC、clang-cl | 已验证 | 尚未验证 | 尚未验证 |
| Linux x64 / GCC | 已验证 | 已验证 | 已验证，前缀构建 |
| Linux ARM64 / GCC、Clang | 已验证 | 尚未验证 | 尚未验证 |
| macOS Apple Silicon / AppleClang | 已验证 | 已验证 | 已验证，前缀构建 |
| macOS Intel / AppleClang | 已验证 | 尚未验证 | 尚未验证 |
| Android / iOS | 尚未设备验证 | 尚未设备验证 | 尚未设备验证 |

已测版本：mimalloc 3.4.3、jemalloc 5.3.1；更新版本需另行验证。[测试结果](../testing.zh-CN.md)

## 选择与构建

配置时显式设置 `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc` 与 `UNIMEMORY_STATISTICS=ON|OFF`。`Memory` 为通用基类，`Global`、`Heap`、`Stack` 均直接构造；Global 实例共用原生分配器，但统计和 PMR 资源身份各自独立。所有后端均支持 Heap。

Heap 创建独立分配组。ON 允许所有 Memory 模式开启 Basic；OFF 拒绝 Basic。统计默认 Disabled。[统计说明](statistics.zh-CN.md)

| 后端 | CMake 配置 | 依赖要求 |
| --- | --- | --- |
| Standard | MUNIMEMORY_BACKEND=standardM | C++20，使用进程的标准分配路径 |
| mimalloc | MUNIMEMORY_BACKEND=mimallocM | 3.4.3+ 头文件和匹配库 |
| jemalloc | MUNIMEMORY_BACKEND=jemallocM | 5.3.1 已验证，显式 Mje_M 导出 |

### Windows

将 MVCPKG_ROOTM 设为现有 vcpkg 路径，在 PowerShell 执行：

MMMpowershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --config Release
MMM

仓库 manifest 固定依赖版本。新建 mimalloc x64 构建默认使用 Mx64-windows-unimemoryM，关闭 mimalloc CRT 重定向；显式指定的 triplet 保持原配置。下载 SDK 依赖需要网络。下方命令选择 mimalloc；选择 jemalloc 时在独立构建目录将后端值改为 MjemallocM。

若复用现有 vcpkg 安装且不改变包集合，在新的构建目录中指定安装根目录和实际 triplet：

MMMpowershell
cmake -S . -B build/backends-existing M
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" M
    "-DVCPKG_INSTALLED_DIR=<installed-root>" "-DVCPKG_TARGET_TRIPLET=<installed-triplet>" M
    -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_MANIFEST_INSTALL=OFF M
    -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
MMM

安装根目录包含各 triplet 子目录，不是 triplet 目录本身。所选后端包必须已经存在，且使用兼容的显式调用构建。不要让不同 manifest 共用同一安装根目录执行安装：vcpkg 可能删除该 manifest 不需要的包。Classic 模式复用所提供的包，不会追溯应用本仓库 manifest 的版本固定。

### Linux / macOS

先安装匹配后端，将占位符换成安装目录：

MMMsh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_BACKEND=mimalloc -DUNIMEMORY_STATISTICS=OFF
cmake --build build/backends --parallel
MMM

jemalloc 必须以 M--with-jemalloc-prefix=je_ --disable-cxxM 构建，默认系统包不能直接使用。仅设置前缀仍可能替换全局 Mnew/deleteM，UniMemory 拒绝这类库。[官方构建选项](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md)

手动构建 mimalloc 时使用 MMI_OVERRIDE=OFFM；Windows 另设 MMI_WIN_REDIRECT=OFFM。应用代码仅包含 M<unimem/all.h>M。

## 动态库部署

| 平台 | 部署方式 |
| --- | --- |
| Windows | 所选 SDK DLL 放在程序目录或 DLL 搜索路径 |
| Linux | 部署所选 SDK M.soM，配置应用的运行时库搜索路径 |
| macOS | 按 dylib 的 install name/rpath 部署 |

UniMemory 是纯头文件库，只需部署动态链接的 SDK 二进制。自定义 Unix 布局使用应用的 MCMAKE_INSTALL_RPATHM；接口目标仍传递所选 SDK 链接，配置使用方时 CMake 也须找到该 SDK。使用匹配的编译器与运行库配置；jemalloc 的 Release/Debug 库分别取自 MlibM/Mdebug/libM，仅有一份时共用。

mimalloc DLL 若启用了 CRT 重定向，可在程序启动前设置 MMIMALLOC_DISABLE_REDIRECT=1M，避免接管整个进程。UniMemory 不在运行中修改环境变量。

[能力对照](../allocator-capabilities.zh-CN.md) · [mimalloc](../backends/mimalloc.zh-CN.md) · [jemalloc](../backends/jemalloc.zh-CN.md)
