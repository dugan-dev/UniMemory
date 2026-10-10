# jemalloc：原生能力清单

[目录](../README.zh-CN.md) · [English](jemalloc.md) · **简体中文**

UniMemory 显式调用 `je_` 分配入口；`Memory::global(Backend::Jemalloc)` 使用共享路径，`Memory::heap(Backend::Jemalloc)` 创建独立 arena。业务代码始终通过统一接口访问。

**版本与条件：**[官方手册](https://jemalloc.net/jemalloc.3.html)描述 **5.4.0**；本仓库验证 **5.3.1** 及 `je_mallocx`/`je_dallocx`。启用需要 `UNIMEMORY_BACKEND=jemalloc` 及匹配库；Linux/macOS 使用 `--with-jemalloc-prefix=je_ --disable-cxx` 构建。前缀不会关闭全局 C++ `new/delete` 替换，Unix 配置会拒绝导出这些替换的库。关闭原生替换不影响 UniMemory 创建 Object。Windows、Linux 已验证；macOS 已通过 GitHub CI；Android/iOS 尚未设备验证。5.4.0 新符号不能假定存在于 5.3.1。[验证范围](../testing.zh-CN.md)

| 功能族 | 代表入口 | 作用范围与取舍 |
| --- | --- | --- |
| 基础分配 | `malloc`、`calloc`、`realloc`、`free`、`aligned_alloc` | 是否替换进程路径由构建/链接决定；UniMemory 使用显式前缀调用 |
| 扩展分配与调整 | `je_mallocx`、`je_rallocx`、`je_xallocx`、`MALLOCX_ALIGN`、`MALLOCX_ZERO` | 对齐、清零、arena/tcache 标志；通用字节接口覆盖常用语义，原地 `xallocx` 保留原生 |
| 大小与高效释放 | `je_sallocx`、`je_dallocx`、`je_sdallocx`、`je_nallocx` | 统一释放接口保留大小；当前使用 `dallocx` |
| Memory::heap() | `arenas.create`、`MALLOCX_ARENA`、`arena.<i>.destroy` | `Memory::heap()` 使用独立 arena；支持整体 reset、collect 和 owns；先销毁 C++ 所有者 |
| 线程缓存 | `tcache.create`、`tcache.flush`、`MALLOCX_TCACHE` | 影响性能与内存占用；公共接口不直接管理 |
| Extent hooks | `arena.<i>.extent_hooks` | OS 内存管理回调需覆盖 arena 寿命；不作为简单字节适配层 |
| 控制与调参 | `je_mallctl`、`je_mallctlnametomib`、`je_mallctlbymib` | 统一回收延迟选项设置新 arena 默认值；其余专有选项不暴露 |
| 统计与画像 | `je_malloc_stats_print`、`stats.*`、`epoch`、`prof.*` | `backend_statistics()` 查询可用指标；画像不在当前公共接口 |
| 回收 | `arena.<i>.purge/decay` | `heap.collect()` 映射 purge，保留有效分配，不保证 RSS 降低 |

业务代码只使用 `<unimem/memory.h>`。详细统计是否可用取决于 jemalloc 的统计编译选项；通过 `capabilities()` 查询。原生统计用于诊断，不建议每次分配都查询。支持条件以[官方手册](https://jemalloc.net/jemalloc.3.html)和实际使用版本为准。

安装包仍依赖 jemalloc。查找或符号验证失败时，`find_package(UniMemory CONFIG QUIET)` 返回 `UniMemory_FOUND=FALSE`，通过 `UniMemory_NOT_FOUND_MESSAGE` 提供原因，不导入 UniMemory 或 jemalloc 目标。`find_package(... REQUIRED)` 和源码中显式启用该后端仍会使配置失败。Windows 分别使用 release/debug 库；缺少 debug 库时回退到 Release。

## 初始化

Global/Heap 创建、能力查询和运行选项修改共用首次初始化保护。在并发 UniMemory 操作进入 jemalloc 前，先完成一次原生版本查询。初始化失败允许后续重试：工厂及运行选项修改通过异常报告失败；noexcept 能力查询报告无详细统计。`available()` 表示编译支持，不代表初始化探测成功。

保护范围是一份已链接的 UniMemory。直接调用 `je_*` 或多份独立封装共享同一原生 DLL 时，仍需自行协调启动。它不代替对象读写同步，也不改变 Heap 回收的独占要求。[冷启动证据](../review-2026-10-08.md#concurrent-jemalloc-initialization)
