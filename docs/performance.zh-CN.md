# 性能报告

[UniMemory](../README.zh-CN.md) · [English](performance.md) · **简体中文**

按使用场景比较耗时与内存占用；结果仅适用于所测配置。

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

未绑定 CPU；小差异可能是噪声。未验证 p95/p99、NUMA、长期碎片、macOS 或移动设备性能。

[方法](benchmarking.zh-CN.md) · [原始数据](results/0.0.1/README.md) · [测试](testing.zh-CN.md)
