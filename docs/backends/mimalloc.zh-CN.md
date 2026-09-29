# mimalloc：原生能力清单

[English](mimalloc.md) · **简体中文**
[文档目录](../README.zh-CN.md) / [能力对照](../allocator-capabilities.zh-CN.md) / mimalloc

UniMemory 将基础分配、对齐、清零、Object 生命周期、分配范围、详细统计和一项回收延迟设置放入统一接口；heap 遍历、OS arena 和 subprocess 仍是上游特有能力，不作为 UniMemory 公共接口。

**版本与条件：**[官方功能目录](https://microsoft.github.io/mimalloc/topics.html)于 2026-09-23 核对，网站展示 3.5/2.5/1.15；本仓库 Windows x64/MSVC 与 Linux x64/GCC/WSL 本地验证 **3.4.3**；本轮 GitHub CI 因账户限制未执行。启用需要 `UNIMEMORY_WITH_MIMALLOC=ON`、v3 头文件和库。Android/iOS 尚未设备验证。当前核心集成已验证默认分配/清零/对齐重分配、`mi_heap_new`、heap 分配/重分配、`mi_free`、`mi_heap_destroy`、`mi_stats_get`；其余函数使用前须核对实际版本与平台。

`Memory::heap()` 另提供整体 `reset()`、闲置 `collect()` 和有效指针 `owns()`；见 [Heap 指南](../guides/heap.zh-CN.md)。

## 功能族与边界

| 官方主题 | 代表入口 | 在 UniMemory 中的处理 |
| --- | --- | --- |
| [基础分配](https://microsoft.github.io/mimalloc/group__malloc.html) | `mi_malloc`、`mi_calloc`、`mi_realloc`、`mi_free` | 普通 `Memory` 使用默认分配接口，`Memory::heap()` 使用独立 heap |
| [对齐分配](https://microsoft.github.io/mimalloc/group__aligned.html) | `mi_malloc_aligned`、`mi_realloc_aligned` | `Memory` 接收并检查对齐 |
| [类型宏](https://microsoft.github.io/mimalloc/group__typed.html) | `mi_malloc_tp`、`mi_free_tp` | C 宏便利层；C++ 业务用模板 Object 接口 |
| [固定大小](https://microsoft.github.io/mimalloc/group__constantsize.html) | `mi_malloc_csize`、`mi_free_csize` | 潜在内联快速路径；保留大小参数，后续按实测优化 |
| [清零重分配](https://microsoft.github.io/mimalloc/group__zeroinit.html) | `mi_rezalloc`、`mi_heap_rezalloc_aligned` | 原生形式对旧块有清零前提；`reallocate_zeroed` 统一为只清零新增请求字节 |
| [Heap](https://microsoft.github.io/mimalloc/group__heap.html) | `mi_heap_new`、`mi_heap_malloc`、`mi_heap_delete`、`mi_heap_destroy` | 仅 `Memory::heap(Backend::Mimalloc)` 创建并管理独立 heap |
| [Heap 遍历](https://microsoft.github.io/mimalloc/group__analysis.html) | `mi_heap_visit_blocks` | 扫描成本高，不作为统一接口 |
| [OS Arena](https://microsoft.github.io/mimalloc/group__arenas.html) | `mi_reserve_os_memory_ex`、`mi_heap_new_in_arena` | 大型 OS 内存区域，不等于 UniMemory `Memory::heap()` 或 `Stack Memory` |
| [Subprocess](https://microsoft.github.io/mimalloc/group__subproc.html) | `mi_subproc_new`、`mi_subproc_add_current_thread` | 具有线程归属规则；不是 `Memory` 的 Backend 切换 |
| [扩展函数](https://microsoft.github.io/mimalloc/group__extended.html) | 所有权、可用大小、收集函数 | Backend 专属，当前公共接口不提供 |
| [统计](https://microsoft.github.io/mimalloc/group__stats.html) | `mi_stats_get` | Global 仅提供 Process 已提交/已预留字节；Heap 无原生详细指标，请求字节使用 Basic |
| [运行选项](https://microsoft.github.io/mimalloc/group__options.html) | `mi_option_get`、`mi_option_set` | 统一的回收延迟选项；其余专有选项不暴露 |
| [线程局部 heap（v3）](https://microsoft.github.io/mimalloc/group__theap.html) | `mi_heap_theap`、`mi_theap_malloc` | 专门快速路径；默认不包装 |
| [POSIX 包装](https://microsoft.github.io/mimalloc/group__posix.html) | `mi_posix_memalign` 等 | C/POSIX 接入与全局替换另行处理 |
| [C++ 包装](https://microsoft.github.io/mimalloc/group__cpp.html) | mimalloc 前缀的 new/delete 形式 | UniMemory 自己提供 Object 与 std Container 适配 |

## 统计准确性

固定版本的默认 Release 为 `MI_STAT=0`，不记录普通 malloc 计数；请求字节与大块释放更新也不能一致地表示在用字节。
Heap 页计数记录在 subprocess 范围。因此仅映射 Process 已提交/已预留指标；mimalloc Heap 的详细统计能力为 false。
两种 Memory 均可开启 Basic 精确请求计数；不可用字段返回 optional 空值。
汇总仅覆盖调用线程当前的原生 subprocess（通常为 main），不汇总其他独立原生 subprocess（[查询源码](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/stats.c#L566)）。

核实源码：[构建条件](https://github.com/microsoft/mimalloc/blob/v3.4.3/include/mimalloc/types.h#L70)、
[分配计数](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/alloc.c#L57)、
[释放计数](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/free.c#L563)、
[OS 页计数](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/os.c#L223)。

## 使用统一接口

业务代码只包含 `<unimem/memory.h>`；查询 `capabilities(Backend::Mimalloc)` 后使用 `Memory::heap()`、`backend_statistics()` 和 `set_runtime_option()`。Backend 统计可能同步或扫描，应与分配热路径分开测量。
