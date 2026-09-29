# Google TCMalloc：进程级接入

[目录](../README.zh-CN.md) · [English](tcmalloc.md) · **简体中文**

Google TCMalloc 由**最终可执行程序链接**到进程分配路径。它不是可逐个 `Memory` Object 选择的 Backend；`Backend::Standard` 使用当时进程的标准 `new` 路径。不要与 gperftools 的另一套 tcmalloc 实现混为一谈。

**证据范围：**2026-09-23 核对了[基础 API](https://google.github.io/tcmalloc/reference.html)、[架构](https://google.github.io/tcmalloc/design.html)、[平台](https://google.github.io/tcmalloc/platforms.html)、[统计](https://google.github.io/tcmalloc/stats.html)和 [`malloc_extension.h` 固定修订 `1c6a831d649134efac38663f5a269a43f0d02702`](https://github.com/google/tcmalloc/blob/1c6a831d649134efac38663f5a269a43f0d02702/tcmalloc/malloc_extension.h)。本机 Linux/WSL 构建该修订，运行上游完整默认测试选择及 UniMemory 的 671 项链接测试。[结果与环境限制](../testing.zh-CN.md)

| 功能族 | 代表入口 | 作用范围与取舍 |
| --- | --- | --- |
| C/C++ 基础分配 | `malloc`、`calloc`、`realloc`、`free`、全局 `new/delete` | 最终程序决定进程路径；不加入 `Backend` 枚举 |
| 对齐与已知大小释放 | aligned new/delete、`aligned_alloc`、`sdallocx`、`nallocx` | 底层可利用大小/对齐；UniMemory 保留对应参数 |
| CPU/线程缓存 | per-CPU 或 per-thread 前端 | Backend 内部的吞吐量/内存占用取舍，不是业务 heap |
| 进程统计 | `MallocExtension::GetStats`、`GetProperties` | 进程范围，与某一 `Memory` 的请求字节不同 |
| 采样画像 | `SnapshotCurrent`、`StartAllocationProfiling`、`StartLifetimeProfiling` | 有采样/分析成本；用于诊断，不在分配热路径 |
| 释放与调参 | `ReleaseMemoryToSystem`、内存限制和缓存大小 | 进程级策略，不放入逐实例 `configure_global()` |
| 调试采样 | guarded sampling / GWP-ASan | 概率性错误检测，不等于通用调试分配器 |

## 平台和构建

[官方平台表](https://google.github.io/tcmalloc/platforms.html)列出 Linux 64 位 x86/AArch64 等支持组合；Windows、macOS、Android、iOS 不在该表支持范围。[官方仓库](https://github.com/google/tcmalloc)以 Bazel 为主，CMake 支持标为实验性。UniMemory 提供 `unimemory_link_tcmalloc(final_executable, allocator_target)` 辅助函数，只允许 Linux，且两个参数必须为已有 CMake 目标。

本仓库使用固定修订的 Bazel `//tcmalloc`，把 UniMemory 静态库链接进最终程序，已验证这条**最终链接路径**；未使用真实 TCMalloc CMake 目标执行上述辅助函数。原生统计、采样和回收分别测量，不混入基础分配延迟。
