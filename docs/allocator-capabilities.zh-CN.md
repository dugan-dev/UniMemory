# 后端能力对照

[目录](README.zh-CN.md) · [English](allocator-capabilities.md) · **简体中文**

后端配置见[平台与后端](guides/backends.zh-CN.md)。

UniMemory 统一**业务常用的分配、所有权、分配范围、诊断和可选调优接口**。可用功能取决于 CMake 所选后端；各分配器保留自己的物理行为。

| 能力 | Standard C++ | mimalloc | jemalloc | UniMemory 接口 |
| --- | --- | --- | --- | --- |
| 基础与对齐分配 | `new/delete`、对齐形式 | `mi_malloc`、对齐函数 | `mallocx`、`MALLOCX_ALIGN` | `allocate/deallocate` |
| 清零分配/扩容 | 手动分配、复制、清零 | `mi_zalloc`、`mi_rezalloc` | `MALLOCX_ZERO` | `allocate_zeroed`、`reallocate_zeroed` |
| Object 与 Container | 标准分配器、PMR | C++ 包装与宏 | 可提供底层存储 | `create/destroy`、Smart Pointer、`Allocator<T>`、PMR |
| 已知大小的释放 | sized delete | `mi_free_csize` | `sdallocx` | 释放接口保留大小，不承诺专用快速路径 |
| 独立 Heap | 无对应统一接口 | heap | arena | `Heap()`，所有后端均支持 |
| 详细统计 | 无分配器内部指标 | 原生 Process 指标 | `mallctl`，Process / Heap 指标 | Basic 请求计数；原生指标使用后端工具 |
| 调参与回收 | 无对应控制接口 | 收集、运行时选项 | purge/decay、`mallctl` | `Memory::config(release_delay_ms)`、Heap 的 `collect()` |
| 临时栈式分配 | 调用方提供缓冲区 | 无对应语义 | 无对应语义 | `Stack(buffer)` |
| 分配画像与块遍历 | 无共同接口 | 原生专属功能 | 原生专属功能 | 0.0.1 不提供 |

## 三种范围

```mermaid
flowchart LR
    A[单个 Memory 的请求] --> B[CMake 所选后端]
    C[请求统计/回收延迟] --> D[按 Memory 模式与所选 Backend 使用]
    E[Stack] --> F[调用方提供的固定缓冲区]
```

| 名词 | 在本库中的准确含义 |
| --- | --- |
| **请求字节** | 传给某个 `Memory` 的大小；不等于可用容量、已提交页或进程 RSS |
| **Heap** | `Heap()` 创建；动态增长，不保证连续或物理隔离 |
| **mimalloc heap** | mimalloc 构建中 UniMemory Heap 使用的原生 SDK heap；OS arena 是另一层大型内存区域 |
| **jemalloc arena** | 原生 SDK arena；Global 使用共享默认路径，UniMemory Heap 使用独立 arena |
| **Stack Memory** | 固定缓冲区上的线性回退；不是线程调用栈，也不析构 C++ Object |

## 证据与版本

已验证 mimalloc **3.4.3**、jemalloc **5.3.1**。官方网页可能描述更新版本，不代表已验证版本支持所有新接口。[构建条件](guides/backends.zh-CN.md) · [测试结果](testing.zh-CN.md)

按 Backend 查完整功能族：[mimalloc](backends/mimalloc.zh-CN.md) · [jemalloc](backends/jemalloc.zh-CN.md)。
