# 分配耗时

[性能](../performance.zh-CN.md) · [English](latency.md) · **简体中文**

**纳秒/完整操作，越小越快。** Global，统计关闭，Release 构建。轮数与统计方法见下方。

<!-- performance-source -->
2026-10-11 · [e758dc2](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387) · [CSV](../results/current/README.md)
<!-- /performance-source -->

## 对象、容器与内存块

### Windows

<!-- performance-table:workloads-windows-x64 -->
| 场景 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique，64 B | 43.6 | 8.1 | 31.4 |
| make_shared，64 B | 49.9 | 12.4 | 50.7 |
| vector，32 个 uint64_t | 611.7 | 174.6 | 428.7 |
| PMR vector，32 个 uint64_t | 559.3 | 184.3 | 432.3 |
| 零初始化 Block，4 KiB | 88.1 | 62.4 | 60.4 |
| Block 扩容，4 → 8 KiB | 162.9 | 104.5 | 132.8 |
| 混合生命周期，4–16 KiB | 114.3 | 29.3 | 127.0 |
| 跨线程释放，64 B，8 线程 | 92.2 | 51.0 | 106.2 |
<!-- /performance-table -->

### Linux

<!-- performance-table:workloads-linux-x64 -->
| 场景 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique，64 B | 6.2 | 7.0 | 6.6 |
| make_shared，64 B | 7.6 | 11.4 | 11.3 |
| vector，32 个 uint64_t | 57.2 | 79.7 | 80.2 |
| PMR vector，32 个 uint64_t | 62.1 | 86.6 | 88.2 |
| 零初始化 Block，4 KiB | 44.8 | 89.1 | 37.3 |
| Block 扩容，4 → 8 KiB | 75.0 | 103.2 | 62.2 |
| 混合生命周期，4–16 KiB | 102.6 | 26.1 | 47.4 |
| 跨线程释放，64 B，8 线程 | 75.7 | 36.0 | 38.6 |
<!-- /performance-table -->

### macOS

<!-- performance-table:workloads-macos-arm64 -->
| 场景 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique，64 B | 18.4 | 4.8 | 17.0 |
| make_shared，64 B | 19.1 | 10.3 | 20.6 |
| vector，32 个 uint64_t | 141.3 | 69.6 | 170.1 |
| PMR vector，32 个 uint64_t | 144.5 | 88.7 | 185.5 |
| 零初始化 Block，4 KiB | 66.8 | 62.2 | 76.5 |
| Block 扩容，4 → 8 KiB | 146.6 | 100.9 | 131.5 |
| 混合生命周期，4–16 KiB | 31.6 | 21.5 | 23.7 |
| 跨线程释放，64 B，8 线程 | 28.8 | 15.9 | 58.5 |
<!-- /performance-table -->

对象包含创建与销毁；vector 增长到 32 个 uint64_t；resize 包含分配、扩容与释放。每行操作不同，只在同一行内比较。

![对象与内存块耗时](../images/workload-comparison.png)

![macOS 对象与内存块耗时](../images/performance/macos-arm64/workloads.zh-CN.svg)

Standard = 1；越短越快。macOS 图展示选定 API 场景，完整八项见上表。

## 原生接口与统计开关

一次分配与释放，16 字节对齐。Native 直接调用后端；API 使用 UniMemory，关闭统计；Basic 开启统计。Native/API 来自同一 OFF 构建，Basic 来自 ON 构建。

### Windows

<!-- performance-table:latency-windows-x64 -->
| 字节数 | 后端 | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 45.6 | 46.2 | 42.7 |
| 16 | mimalloc | 7.3 | 7.2 | 9.8 |
| 16 | jemalloc | 32.9 | 33.5 | 43.9 |
| 64 | Standard | 45.0 | 46.1 | 44.0 |
| 64 | mimalloc | 6.8 | 7.6 | 10.0 |
| 64 | jemalloc | 33.2 | 33.0 | 41.6 |
| 256 | Standard | 47.5 | 48.2 | 46.2 |
| 256 | mimalloc | 7.8 | 8.5 | 10.9 |
| 256 | jemalloc | 33.4 | 33.6 | 44.0 |
| 4096 | Standard | 47.5 | 49.0 | 44.7 |
| 4096 | mimalloc | 28.7 | 29.4 | 30.6 |
| 4096 | jemalloc | 35.9 | 36.5 | 46.9 |
| 65536 | Standard | 125.4 | 128.4 | 100.7 |
| 65536 | mimalloc | 31.5 | 32.2 | 33.4 |
| 65536 | jemalloc | 289.7 | 289.6 | 331.5 |
<!-- /performance-table -->

### Linux

<!-- performance-table:latency-linux-x64 -->
| 字节数 | 后端 | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 8.7 | 8.8 | 26.2 |
| 16 | mimalloc | 8.4 | 9.4 | 10.4 |
| 16 | jemalloc | 11.3 | 11.3 | 16.3 |
| 64 | Standard | 8.5 | 9.0 | 25.9 |
| 64 | mimalloc | 8.8 | 9.5 | 10.3 |
| 64 | jemalloc | 10.3 | 10.9 | 16.5 |
| 256 | Standard | 8.8 | 9.4 | 25.8 |
| 256 | mimalloc | 9.8 | 10.2 | 10.9 |
| 256 | jemalloc | 10.4 | 10.8 | 16.7 |
| 4096 | Standard | 15.9 | 15.9 | 31.4 |
| 4096 | mimalloc | 24.1 | 24.4 | 24.6 |
| 4096 | jemalloc | 12.9 | 12.9 | 18.9 |
| 65536 | Standard | 17.3 | 17.4 | 31.7 |
| 65536 | mimalloc | 25.3 | 25.3 | 26.5 |
| 65536 | jemalloc | 193.5 | 192.4 | 253.2 |
<!-- /performance-table -->

### macOS

<!-- performance-table:latency-macos-arm64 -->
| 字节数 | 后端 | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 16 | Standard | 16.5 | 18.9 | 19.8 |
| 16 | mimalloc | 5.0 | 5.3 | 6.7 |
| 16 | jemalloc | 15.2 | 16.7 | 18.1 |
| 64 | Standard | 19.6 | 20.8 | 24.2 |
| 64 | mimalloc | 5.2 | 5.7 | 6.9 |
| 64 | jemalloc | 16.0 | 21.3 | 18.7 |
| 256 | Standard | 19.5 | 20.7 | 20.2 |
| 256 | mimalloc | 6.8 | 6.8 | 7.9 |
| 256 | jemalloc | 16.1 | 15.6 | 23.3 |
| 4096 | Standard | 22.3 | 22.5 | 26.3 |
| 4096 | mimalloc | 14.7 | 15.3 | 20.1 |
| 4096 | jemalloc | 22.3 | 20.8 | 27.2 |
| 65536 | Standard | 27.4 | 25.4 | 26.5 |
| 65536 | mimalloc | 16.6 | 16.7 | 30.1 |
| 65536 | jemalloc | 248.3 | 262.3 | 297.3 |
<!-- /performance-table -->

小差异可能是测量噪声；后端和尺寸都会影响统计成本。各路径的成功请求大小与对齐一致。

## 跨线程释放

一条线程分配 64 B，工作线程释放。线程创建在计时前，等待完成在计时内。

<!-- performance-table:cross-thread -->
| 平台 | 工作线程 | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 63.7 | 30.2 | 80.2 |
| Windows | 4 | 84.8 | 34.9 | 92.0 |
| Windows | 8 | 92.2 | 51.0 | 106.2 |
| Linux | 2 | 65.6 | 22.6 | 21.8 |
| Linux | 4 | 81.8 | 23.7 | 29.1 |
| Linux | 8 | 75.7 | 36.0 | 38.6 |
| macOS | 2 | 26.2 | 14.2 | 49.9 |
| macOS | 4 | 26.1 | 14.4 | 45.4 |
| macOS | 8 | 28.8 | 15.9 | 58.5 |
<!-- /performance-table -->

## 栈式分配

预先准备 Buffer，一次 mark/allocate/rewind，16 字节对齐，只写入一个字节。它与 Heap 分配的工作量不同。

<!-- performance-table:stack -->
| 平台 | 字节数 | 纳秒/轮 |
| --- | --- | --- |
| Windows | 64 | 3.39 |
| Windows | 4096 | 3.39 |
| Linux | 64 | 3.84 |
| Linux | 4096 | 3.83 |
| macOS | 64 | 4.33 |
| macOS | 4096 | 3.91 |
<!-- /performance-table -->

未绑定 CPU。此页报告完整操作的中位数；单次分配的 p95/p99 见性能主页。

[方法](../benchmarking.zh-CN.md) · [原始数据](../results/current/README.md) · [内存](memory.zh-CN.md)
