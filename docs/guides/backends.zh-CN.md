# 平台与后端

[目录](../README.zh-CN.md) · [English](backends.md) · **简体中文**

## 平台验证状态

| 平台 | Standard | mimalloc | jemalloc | Google TCMalloc |
| --- | --- | --- | --- | --- |
| Windows x64 / MSVC | 已验证 | 已验证 | 已验证 | 上游未列支持 |
| Linux x64 / GCC | 已验证 | 已验证 | 已验证，前缀构建 | Linux/WSL 链接已验证 |
| macOS / Apple Clang | GitHub CI | GitHub CI | GitHub CI，前缀构建 | 上游未列支持 |
| Android / iOS | 尚未设备验证 | 尚未设备验证 | 尚未设备验证 | 上游未列支持 |

已测版本：mimalloc 3.4.3、jemalloc 5.3.1；更新版本需另行验证。[测试结果](../testing.zh-CN.md)

## 选择与构建

Global 每个后端共享一个 Memory，Heap 是独立分配组。后端在获取或创建时确定，不在该实例上切换；未启用的后端会报错，不自动回退。

| 后端 | CMake 配置 | 依赖要求 |
| --- | --- | --- |
| Standard | 默认启用 | C++20，使用进程的标准分配路径 |
| mimalloc | `UNIMEMORY_WITH_MIMALLOC=ON` | 3.4.3+ 头文件和匹配库 |
| jemalloc | `UNIMEMORY_WITH_JEMALLOC=ON` | 5.3.1 已验证，显式 `je_` 导出 |
| Google TCMalloc | 最终程序链接 | Linux，影响 Standard 路径，不是 Backend 枚举 |

### Windows

将 `VCPKG_ROOT` 设为现有 vcpkg 路径，在 PowerShell 执行：

```powershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON
cmake --build build/backends --config Release
```

仓库 manifest 固定依赖版本。新建 x64 构建默认使用 `x64-windows-unimemory`，关闭 mimalloc CRT 重定向；显式指定的 triplet 保持原配置。下载可选依赖需要网络。

### Linux / macOS

先安装匹配后端，将占位符换成安装目录：

```sh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON
cmake --build build/backends --parallel
```

jemalloc 必须以 `--with-jemalloc-prefix=je_ --disable-cxx` 构建，默认系统包不能直接使用。仅设置前缀仍可能替换全局 `new/delete`，UniMemory 拒绝这类库。[官方构建选项](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md)

手动构建 mimalloc 时使用 `MI_OVERRIDE=OFF`；Windows 另设 `MI_WIN_REDIRECT=OFF`。应用代码仅包含 `<unimem/memory.h>`。

## 动态库部署

| 平台 | 部署方式 |
| --- | --- |
| Windows | UniMemory 和所需 DLL 放在程序目录或 DLL 搜索路径 |
| Linux | 私有安装默认使用 `$ORIGIN`，后端 `.so` 可与 `libUniMemory.so` 同目录 |
| macOS | 按 dylib 的 install name/rpath 部署 |

自定义 Unix 布局使用 `CMAKE_INSTALL_RPATH`。配置使用方项目时，CMake 也须能找到可选后端。使用匹配的编译器与运行库配置；jemalloc 的 Release/Debug 库分别取自 `lib`/`debug/lib`，仅有一份时共用。

mimalloc DLL 若启用了 CRT 重定向，可在程序启动前设置 `MIMALLOC_DISABLE_REDIRECT=1`，避免接管整个进程。UniMemory 不在运行中修改环境变量。

[能力对照](../allocator-capabilities.zh-CN.md) · [mimalloc](../backends/mimalloc.zh-CN.md) · [jemalloc](../backends/jemalloc.zh-CN.md) · [TCMalloc](../backends/tcmalloc.zh-CN.md)
