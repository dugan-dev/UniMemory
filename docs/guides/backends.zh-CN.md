# Backend 与平台

[English](backends.md) · **简体中文**
[文档目录](../README.zh-CN.md) / 进阶

**默认路径随装随用。** 获取 Global 或创建 Heap 时选择 Backend；选择之后不在运行中切换该实例。可选 Backend 必须先在构建时启用。

```cpp
#include <unimem/memory.h>

unimem::Memory& standard = unimem::Memory::global();
if (unimem::capabilities(unimem::Backend::Mimalloc).available) {
    unimem::Memory& selected = unimem::Memory::global(unimem::Backend::Mimalloc);
    unimem::OwnedBlock block = selected.make_block(1024);
}
```

Backend 是否更快需要针对具体负载测量。请求未编入的 Backend 会失败，不会静默回退到标准 Backend。

## 选择与构建

| 路径 | 开启方式 | 作用范围 |
| --- | --- | --- |
| 标准 | 默认 | 此 `Memory` 使用系统/进程默认 C++ 分配路径 |
| mimalloc v3 | `-DUNIMEMORY_WITH_MIMALLOC=ON` | 此 `Memory` 使用默认 mimalloc 分配接口；只有 Memory::heap() 创建独立 heap |
| jemalloc `je_` | `-DUNIMEMORY_WITH_JEMALLOC=ON` | 此 `Memory` 显式使用所选 Backend；`Memory::heap()` 使用独立 Heap |
| Google TCMalloc | 在 Linux 最终程序中链接 | 进程级替换；**不是** `Backend` 枚举值 |

```cmake
# Enable optional backends when building UniMemory; link the same public target.
target_link_libraries(my_app PRIVATE UniMemory::UniMemory)
```

Windows 可使用仓库的 vcpkg manifest feature 获取 mimalloc/jemalloc 依赖。Linux/macOS 上 jemalloc 必须提供 `je_` 前缀导出。启用 mimalloc/jemalloc 仍须有匹配的头文件和库；不会自动替换程序所有 `new`。

Windows：将 `VCPKG_ROOT` 设置为现有 vcpkg 路径，再在 PowerShell 执行：

```powershell
cmake -S . -B build/backends "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON
cmake --build build/backends --config Release
ctest --test-dir build/backends -C Release --output-on-failure
```

Linux/macOS：先安装匹配的原生包，将占位符换成其安装目录：

```sh
cmake -S . -B build/backends -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<allocator-install-prefix>" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON
cmake --build build/backends --parallel
ctest --test-dir build/backends --output-on-failure
```

jemalloc 使用 `--with-jemalloc-prefix=je_ --disable-cxx` 构建；默认系统包不能直接接入。仅设置前缀仍会替换全局 C++ `new/delete`，因此 Unix 配置会检查符号并拒绝这种构建，安装包的使用方也会检查。关闭原生替换不影响 UniMemory 的 Object 创建接口。[官方构建选项](https://github.com/jemalloc/jemalloc/blob/5.3.1/INSTALL.md)。

mimalloc 当前最低要求 3.4.3，更新版本需另行验证。可选依赖下载需要网络；Standard 不需要。

## 平台验证状态

| 平台 | 标准路径 | mimalloc | jemalloc | Google TCMalloc |
| --- | --- | --- | --- | --- |
| Windows x64/MSVC | 已验证 | 3.4.3 已验证 | 5.3.1 已验证 | 上游未列支持 |
| Linux | 最终本地验证 | 3.4.3 最终本地验证 | 5.3.1 `je_` 前缀，最终本地验证 | 本地 Linux/WSL 已验证；最新 CI 尚未完成 |
| macOS / Apple Clang | GitHub CI | GitHub CI | GitHub CI，前缀构建 | 上游未列支持 |
| Android / iOS | 尚未设备验证 | 未承诺 | 未承诺 | 上游未列支持 |

上表描述**本仓库的验证范围**，不是对其他平台性能或可构建性的推断。详情与官方来源见[能力对照](../allocator-capabilities.zh-CN.md)。

## 动态库部署

启用动态 Backend 时，部署其运行库以及 UniMemory。Linux 的私有安装默认使用 `$ORIGIN`，可将 Backend `.so` 放在 `libUniMemory.so` 同目录；系统安装也可配置系统动态库搜索路径。自定义布局通过 `CMAKE_INSTALL_RPATH` 设置。Windows 将所需 DLL 放在程序目录或已配置的搜索路径中。

Windows 的 mimalloc DLL 重定向可以接管整个进程的 CRT，连 Standard 路径也随之改变。新建 Windows x64/vcpkg 构建默认选择 `x64-windows-unimemory`，只关闭 mimalloc 的 `MI_WIN_REDIRECT`，不更换版本。显式指定的用户 triplet 保持原配置；现有构建可改用这个 profile，或在启动程序之前设置 `MIMALLOC_DISABLE_REDIRECT=1`。手动构建 mimalloc 时使用 `MI_OVERRIDE=OFF` 和 `MI_WIN_REDIRECT=OFF`。本库不在运行中修改进程环境。

安装包、静态库与动态库验证见[测试报告](../testing.zh-CN.md)。

jemalloc 导入目标按配置选择 `lib` 或 `debug/lib`；仅有一份库时作为通用依赖。不要把 Debug Backend 的结果当作 Release 性能。

## 统一的可选能力

| 需求 | 接口 |
| --- | --- |
| 独立 Heap | `Memory heap = Memory::heap(backend)` |
| 详细统计 | `capabilities(backend).detailed_statistics` 后调用 `memory.backend_statistics()` |
| 闲置页回收延迟 | `supports(backend, RuntimeOption::UnusedPageReleaseDelayMs)` 后调用 `set_runtime_option(...)` |
| Google TCMalloc 进程替换 | 在支持的 Linux 最终程序中链接；有目标时可调用 `unimemory_link_tcmalloc(app, target)` |

业务代码只需包含 `<unimem/memory.h>`。Backend 参考：[mimalloc](../backends/mimalloc.zh-CN.md) · [jemalloc](../backends/jemalloc.zh-CN.md) · [TCMalloc](../backends/tcmalloc.zh-CN.md)。
