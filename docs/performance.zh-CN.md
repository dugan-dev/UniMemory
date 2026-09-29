# 性能报告

[UniMemory](../README.zh-CN.md) · [English](performance.md) · **简体中文**

分别比较耗时、内存和独立原生程序。所有结论只适用于测量的负载和配置。

| Topic | 内容 |
| --- | --- |
| [1 · Time](performance/latency.zh-CN.md) | Object, Container, resize, handoff, Native/API/Basic |
| [2 · Memory](performance/memory.zh-CN.md) | RSS, retention, size rounding, Heap collect, type sizes |
| [3 · Native programs](performance/applications.zh-CN.md) | 独立程序耗时与峰值内存；不包含 UniMemory 封装 |

## 环境

| Item | Configuration |
| --- | --- |
| Version / measured date (UTC) | 0.0.1 / 2026-09-28 |
| CPU | Xeon w9-3595X, 120 logical CPUs |
| Windows | x64, MSVC 19.44, Release, no LTO |
| Linux | Ubuntu 24.04 / WSL2, GCC 13.3, Release, no LTO |
| Backend | mimalloc 3.4.3; jemalloc 5.3.1 |
| Isolation | mimalloc override/CRT redirect off; Unix jemalloc je_ + --disable-cxx |
| Statistics | Disabled unless labeled Basic |

## 覆盖范围

| Probe | Scope |
| --- | --- |
| Native / API / Basic | 2 OS × 3 Backends × 3 paths × 5 sizes × 3 process trials |
| API sweep | 1001 scenarios / OS, 17 workloads, 3 process trials, 7 repetitions |
| Mixed-size memory | 54 process trials, 270 phase snapshots, content/alignment/count checks |
| Native programs | 16 programs × 3 native allocators × 3 trials |

## 解读

| 观察 | 含义 |
| --- | --- |
| API 有小量成本 | 对齐检查与运行时后端调用不是免费操作 |
| Basic 增加耗时 | 原子计数需要执行；关闭时不更新 |
| 更快未必更省内存 | 缓存与回收策略决定不同负载的取舍 |

未绑定 CPU；小差异可能是噪声。未验证 p95/p99、NUMA、长期碎片、macOS 或移动设备性能。

[Method](benchmarking.zh-CN.md) · [Raw data](results/0.0.1/README.md) · [Tests](testing.zh-CN.md)
