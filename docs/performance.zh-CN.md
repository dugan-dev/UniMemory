# 性能报告

[UniMemory](../README.zh-CN.md) · [English](performance.md) · **简体中文**

按使用场景比较耗时与内存占用；结果仅适用于所测配置。

## 当前远程报告

各平台对比 Standard、mimalloc、jemalloc 原生接口与 UniMemory，包含统计开销、
三轮独立进程测量与实际 CPU 容量。源码修订和结果解读见各平台分析，
完整环境清单见下方当前测量数据。

| 平台 | 分析 | 扩展性 | 内存 | 延迟 | 统计开销 | 内存保留 | 使用场景 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Linux x64 | [报告](images/performance/linux-x64/README.md) | [图表](images/performance/linux-x64/throughput.svg) | [图表](images/performance/linux-x64/memory.svg) | [图表](images/performance/linux-x64/latency.svg) | [图表](images/performance/linux-x64/statistics.svg) | [图表](images/performance/linux-x64/retention.svg) | [图表](images/performance/linux-x64/workloads.svg) |
| Windows x64 | [报告](images/performance/windows-x64/README.md) | [图表](images/performance/windows-x64/throughput.svg) | [图表](images/performance/windows-x64/memory.svg) | [图表](images/performance/windows-x64/latency.svg) | [图表](images/performance/windows-x64/statistics.svg) | [图表](images/performance/windows-x64/retention.svg) | [图表](images/performance/windows-x64/workloads.svg) |
| macOS ARM64 | [报告](images/performance/macos-arm64/README.md) | [图表](images/performance/macos-arm64/throughput.svg) | [图表](images/performance/macos-arm64/memory.svg) | [图表](images/performance/macos-arm64/latency.svg) | [图表](images/performance/macos-arm64/statistics.svg) | [图表](images/performance/macos-arm64/retention.svg) | [图表](images/performance/macos-arm64/workloads.svg) |

[当前测量数据](results/current/)包含校验后的 CSV 与环境清单。逐操作样本在所链接的
Actions 产物中保留 14 天，外部原生应用结果也上传至该流程；汇总数据保存在仓库。
共享机器上的性能波动用于比较分析，
不作为绝对速度门槛。[方法与自动更新](remote-validation.md#performance-and-charts)

## 历史报告 · 2026-09-28

这些是 **2026-09-28 的历史测量**，不是十月修复后的新数据。库版本仍为 0.0.1，实际测量构建由原始数据中的源码和可执行文件哈希标识。

| 主题 | 内容 |
| --- | --- |
| [1 · 耗时](performance/latency.zh-CN.md) | 对象、容器、扩容、跨线程与统计开关 |
| [2 · 内存](performance/memory.zh-CN.md) | 进程占用、释放后保留、Heap 回收与类型大小 |
| [3 · 原生程序](performance/applications.zh-CN.md) | 独立程序耗时与峰值内存；不包含 UniMemory 封装 |

## 环境

| 项目 | 配置 |
| --- | --- |
| 版本 / 测量日期（UTC） | 0.0.1 / 2026-09-28 |
| CPU | Xeon w9-3595X, 120 logical CPUs |
| Windows | x64, MSVC 19.44, Release, no LTO |
| Linux | Ubuntu 24.04 / WSL2, GCC 13.3, Release, no LTO |
| Backend | mimalloc 3.4.3; jemalloc 5.3.1 |
| 统计 | 默认关闭，Basic 表示开启 |

## 覆盖范围

| 测量 | 范围 |
| --- | --- |
| 分配与释放 | 原生接口、统计关闭、统计开启；5 种大小 |
| API 场景 | 每平台 1001 项，覆盖 17 种负载 |
| 内存占用 | 混合大小、不同生命周期，270 个阶段快照 |
| 原生程序 | 16 个程序 × 3 个后端 × 3 次运行 |

## 解读

| 观察 | 含义 |
| --- | --- |
| API 与原生接口 | 统一接口有额外调用成本 |
| 统计开关 | 开启统计需要计数；关闭不计数 |
| 后端选择 | 分配速度与内存保留各有取舍 |

历史测量未绑定 CPU；小差异可能是噪声。这份历史报告未验证 p95/p99、NUMA、长期碎片、macOS 或移动设备性能。

当前没有覆盖全仓库的延迟或内存回归预算。评估性能变更前，应明确负载、基线修订、原生对照、编译器和后端配置、可接受波动及时间/内存限值，再重跑该负载并保留原始多轮结果。API 测试夹具和原生基准程序不能证明生产应用采用情况，也不能证明某后端对实际应用最优。

[方法](benchmarking.zh-CN.md) · [原始数据](results/0.0.1/README.md) · [测试](testing.zh-CN.md)
