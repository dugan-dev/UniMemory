# 内存占用

[性能](../performance.zh-CN.md) · [English](memory.md) · **简体中文**

**进程常驻内存相对基线的增量，MiB。** 每次测量先减去该进程基线，再对三个独立进程取中位数；统计关闭。

<!-- performance-source -->
2026-10-11 · [e758dc2](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387) · [CSV](../results/current/README.md)
<!-- /performance-source -->

请求字节表示业务仍需要的存储；常驻内存还包括分配器缓存与管理信息。释放 Block 后，分配器可以保留空间供后续复用。

## 混合尺寸与生命周期

16384 个 Block，十种 17–8193 B 尺寸，八轮重新分配与释放；保留 1024 个 Block，全部请求页均写入。

操作后立即取样，不额外等待或主动回收。五列依次对应初次分配、首次保留、八轮后的分配、八轮后的保留、全部释放。

在用请求存储，MiB；各平台使用相同负载：

<!-- performance-table:pressure-requested -->
| 初次分配 | 首次保留 | 最终分配 | 最终保留 | 全部释放 |
| --- | --- | --- | --- | --- |
| 25.18 | 1.61 | 25.81 | 1.61 | 0.00 |
<!-- /performance-table -->

### Windows

<!-- performance-table:pressure-windows-x64 -->
| 后端 | 路径 | 初次分配 | 首次保留 | 最终分配 | 最终保留 | 全部释放 |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | 原生 | 27.89 | 26.27 | 28.90 | 26.41 | 1.47 |
| Standard | UniMemory | 27.89 | 26.26 | 28.90 | 26.42 | 1.47 |
| mimalloc | 原生 | 31.73 | 31.79 | 46.46 | 46.46 | 46.46 |
| mimalloc | UniMemory | 31.72 | 31.78 | 46.45 | 46.45 | 46.45 |
| jemalloc | 原生 | 32.42 | 32.49 | 33.68 | 33.68 | 33.68 |
| jemalloc | UniMemory | 32.40 | 32.46 | 33.73 | 33.73 | 33.73 |
<!-- /performance-table -->

### Linux

<!-- performance-table:pressure-linux-x64 -->
| 后端 | 路径 | 初次分配 | 首次保留 | 最终分配 | 最终保留 | 全部释放 |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | 原生 | 25.67 | 25.86 | 26.50 | 26.50 | 26.50 |
| Standard | UniMemory | 25.67 | 25.86 | 26.50 | 26.50 | 26.50 |
| mimalloc | 原生 | 34.20 | 34.39 | 50.39 | 50.39 | 50.39 |
| mimalloc | UniMemory | 34.20 | 34.39 | 50.39 | 50.39 | 50.39 |
| jemalloc | 原生 | 31.20 | 31.32 | 31.32 | 31.32 | 31.32 |
| jemalloc | UniMemory | 31.20 | 31.32 | 31.32 | 31.32 | 31.32 |
<!-- /performance-table -->

### macOS

<!-- performance-table:pressure-macos-arm64 -->
| 后端 | 路径 | 初次分配 | 首次保留 | 最终分配 | 最终保留 | 全部释放 |
| --- | --- | --- | --- | --- | --- | --- |
| Standard | 原生 | 28.61 | 28.61 | 34.44 | 34.44 | 18.44 |
| Standard | UniMemory | 28.61 | 28.61 | 36.47 | 36.47 | 20.47 |
| mimalloc | 原生 | 33.31 | 33.31 | 48.55 | 48.55 | 48.55 |
| mimalloc | UniMemory | 33.31 | 33.31 | 48.55 | 48.55 | 48.55 |
| jemalloc | 原生 | 31.80 | 31.80 | 33.12 | 33.12 | 33.12 |
| jemalloc | UniMemory | 31.78 | 31.78 | 33.09 | 33.09 | 33.09 |
<!-- /performance-table -->

![Windows 混合生命周期内存](../images/memory-retention.png)

![Linux 混合生命周期内存](../images/performance/linux-x64/retention.zh-CN.svg)

![macOS 混合生命周期内存](../images/performance/macos-arm64/retention.zh-CN.svg)

图连接阶段快照，不是连续时间采样。释放后的存储可以继续驻留；保留量本身不能当作泄漏或碎片率。

## 尺寸取整

初次分配时，可用容量比请求字节多出的比例。Standard 没有可移植的容量查询；该指标不包含管理信息与缓存。

<!-- performance-table:rounding -->
| 平台 | 后端 | 路径 | 额外可用容量 |
| --- | --- | --- | --- |
| Windows | Standard | 原生 | — |
| Windows | Standard | UniMemory | — |
| Windows | mimalloc | 原生 | 25.04% |
| Windows | mimalloc | UniMemory | 25.04% |
| Windows | jemalloc | 原生 | 25.04% |
| Windows | jemalloc | UniMemory | 25.04% |
| Linux | Standard | 原生 | — |
| Linux | Standard | UniMemory | — |
| Linux | mimalloc | 原生 | 25.04% |
| Linux | mimalloc | UniMemory | 25.04% |
| Linux | jemalloc | 原生 | 25.04% |
| Linux | jemalloc | UniMemory | 25.04% |
| macOS | Standard | 原生 | — |
| macOS | Standard | UniMemory | — |
| macOS | mimalloc | 原生 | 25.04% |
| macOS | mimalloc | UniMemory | 25.04% |
| macOS | jemalloc | 原生 | 25.04% |
| macOS | jemalloc | UniMemory | 25.04% |
<!-- /performance-table -->

## Heap collect

16384 × 4096 B，共 64 MiB，全部写入。逐块释放后，仅独立 Heap 调用 collect()。

<!-- performance-table:footprint -->
| 平台 | 后端 | 模式 | 分配后 | 释放后 | collect 后 |
| --- | --- | --- | --- | --- | --- |
| Windows | Standard | Global（全局） | 69.61 | 0.29 | — |
| Windows | mimalloc | Global（全局） | 64.31 | 64.32 | — |
| Windows | mimalloc | Heap（独立） | 64.29 | 64.29 | 0.30 |
| Windows | jemalloc | Global（全局） | 72.05 | 72.05 | — |
| Windows | jemalloc | Heap（独立） | 72.09 | 72.09 | 8.18 |
| Linux | Standard | Global（全局） | 64.38 | 0.25 | — |
| Linux | mimalloc | Global（全局） | 66.14 | 66.14 | — |
| Linux | mimalloc | Heap（独立） | 66.13 | 66.13 | 2.13 |
| Linux | jemalloc | Global（全局） | 65.12 | 65.12 | — |
| Linux | jemalloc | Heap（独立） | 67.12 | 67.12 | 3.25 |
| macOS | Standard | Global（全局） | 64.28 | 8.28 | — |
| macOS | mimalloc | Global（全局） | 64.34 | 64.34 | — |
| macOS | mimalloc | Heap（独立） | 64.30 | 64.30 | 64.30 |
| macOS | jemalloc | Global（全局） | 65.53 | 65.53 | — |
| macOS | jemalloc | Heap（独立） | 65.55 | 65.55 | 1.55 |
<!-- /performance-table -->

collect() 回收符合后端策略的闲置资源，不销毁在用对象，也不保证占用立即降低。

## 库自身的存储

<!-- performance-table:storage -->
| 平台 | Memory / B | OwnedBlock / B | `Allocator<T>` / B |
| --- | --- | --- | --- |
| Windows | 80 | 32 | 8 |
| Linux | 80 | 32 | 8 |
| macOS | 80 | 32 | 8 |
<!-- /performance-table -->

类型大小来自当前构建环境；不含 Global 共享状态、Basic 计数或后端管理空间。Stack 创建不额外分配管理状态。

Linux 使用 smaps_rollup RSS，Windows 使用工作集，macOS 使用 resident size；OS 峰值计数与阶段快照不同。

[方法](../benchmarking.zh-CN.md) · [原始数据](../results/current/README.md) · [原生程序](applications.zh-CN.md)
