# 内存占用

[Performance](../performance.zh-CN.md) · [English](memory.md) · **简体中文**

**进程常驻内存相对基线的增量，MiB。** 每种情况运行三个独立进程，取中位数；统计关闭。

请求字节表示业务仍需要的存储；常驻内存表示程序实际占用的物理内存，包含分配器管理的空间和管理信息。释放 Block 后，业务不能再使用它，但分配器可以保留空间供后续分配复用。

## 1 · 混合尺寸与生命周期

16384 个 Block，十种 17–8193 B 尺寸，八轮分配/释放。初始请求 25.18 MiB；保留 1024 个 Block 时请求 1.61 MiB；最后请求为零。检查对齐、内容、计数，并写入请求页。

操作结束后立即取样，不额外等待，也不主动回收。此项检查大量分配后立即保留多少内存，不代表长期内存占用。

```mermaid
flowchart LR
    A[Fill 16384 Blocks] --> B[Keep 1024]
    B --> C[Refill and free: 8 cycles]
    C --> D[Keep 1024]
    D --> E[Free all]
```

### Windows

| Backend | Path | Fill | Keep 1024 | Free all |
| --- | --- | --- | --- | --- |
| Standard | Native | 27.87 | 25.93 | 1.45 |
| Standard | UniMemory | 27.86 | 25.92 | 1.45 |
| mimalloc | Native | 31.71 | 46.43 | 46.44 |
| mimalloc | UniMemory | 31.70 | 46.43 | 46.43 |
| jemalloc | Native | 32.23 | 33.49 | 33.49 |
| jemalloc | UniMemory | 32.21 | 33.46 | 33.46 |

### Linux / WSL

| Backend | Path | Fill | Keep 1024 | Free all |
| --- | --- | --- | --- | --- |
| Standard | Native | 25.59 | 26.37 | 26.37 |
| Standard | UniMemory | 25.59 | 26.37 | 26.37 |
| mimalloc | Native | 34.19 | 50.32 | 50.32 |
| mimalloc | UniMemory | 34.19 | 50.32 | 50.32 |
| jemalloc | Native | 31.97 | 33.30 | 33.30 |
| jemalloc | UniMemory | 31.97 | 33.30 | 33.30 |

![Native and UniMemory memory usage](../images/memory-retention.png)

图中连接阶段快照，不是连续时间采样。释放后缓存可保留用于复用；RSS 包括元数据及进程状态，不能直接当作泄漏或碎片率。

此 Windows 场景中 Standard 释放后立即保留得更少。Native 与 UniMemory 数值接近，未见封装层显著增加此负载的内存占用。该测量没有进一步分离缓存、管理信息和未归还的内存页。

## 2 · 尺寸取整

可用容量可能大于请求大小。该刻意选择尺寸的场景中，mimalloc、jemalloc 的 Native/API 可用容量均比请求多 **25.04%**。Standard 无可移植的容量查询。该值不包含元数据和缓存，也不是外部碎片率。

## 3 · Heap collect

16384 × 4096 B，共 64 MiB，全部写入。逐块释放后，仅独立 Heap 调用 collect()。

| Platform | Backend | Kind | Live | Free | Collect |
| --- | --- | --- | --- | --- | --- |
| Windows | Standard | Global | 69.61 | 0.29 | — |
| Windows | mimalloc | Global | 64.31 | 64.32 | — |
| Windows | mimalloc | Heap | 64.29 | 64.29 | 0.29 |
| Windows | jemalloc | Global | 70.52 | 70.52 | — |
| Windows | jemalloc | Heap | 70.09 | 70.09 | 6.12 |
| Linux | Standard | Global | 64.38 | 0.25 | — |
| Linux | mimalloc | Global | 66.14 | 66.14 | — |
| Linux | mimalloc | Heap | 66.13 | 66.13 | 2.13 |
| Linux | jemalloc | Global | 66.32 | 66.32 | — |
| Linux | jemalloc | Heap | 66.32 | 66.32 | 2.43 |

collect() 回收符合后端策略的闲置资源，不销毁在用 Object，也不保证进程占用立即降低。

## 4 · 库自身的存储

| Type | x64 bytes |
| --- | --- |
| Memory | 80 |
| OwnedBlock | 32 |
| Allocator<T> | 8 |

Memory 内置 Stack 状态及 PMR Resource，创建 Stack 不再分配管理状态。Global 注册表长期保留；Basic 有单独计数状态，原生 Heap 也有自身元数据。表格不包含这些额外状态。对象大小依赖 ABI。

Linux 使用 smaps_rollup RSS，Windows 使用工作集。OS 峰值计数与阶段 RSS 快照不同。

[Method](../benchmarking.zh-CN.md) · [Raw data](../results/0.0.1/README.md) · [Native programs](applications.zh-CN.md)
