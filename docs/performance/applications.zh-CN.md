# 原生程序

[Performance](../performance.zh-CN.md) · [English](applications.md) · **简体中文**

来自 mimalloc-bench 的独立 Linux 程序，进程级替换分配器。**比较 Native Backend，不测 UniMemory 封装成本。**

[Pinned benchmark collection](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

每个程序运行三次，取中位数。这里的系统分配器是 glibc。

## 1 · 耗时

**秒，越低越好。**

| 程序 | glibc | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| cfrac | 4.81 | 4.55 | 4.55 |
| espresso | 6.01 | 5.58 | 5.45 |
| barnes | 2.48 | 2.51 | 2.50 |
| malloc-large | 3.43 | 2.82 | 15.28 |
| mstress | 0.32 | 0.27 | 0.53 |
| cache-thrash | 0.42 | 0.43 | 0.38 |

## 2 · 峰值内存

**峰值 RSS，MiB，越低越好。**

| 程序 | glibc | mimalloc | jemalloc |
| --- | ---: | ---: | ---: |
| cfrac | 2.81 | 6.05 | 4.69 |
| espresso | 1.88 | 12.04 | 4.69 |
| barnes | 56.25 | 64.95 | 58.12 |
| malloc-large | 521.64 | 595.15 | 428.38 |
| mstress | 74.57 | 137.14 | 48.46 |
| cache-thrash | 2.81 | 8.02 | 3.75 |

## 3 · 如何解读

例如 malloc-large 中，mimalloc 耗时较低，jemalloc 峰值内存较低。**更快不一定更省内存。**

合集包含计算、逻辑、大片内存、跨线程和 Cache 负载。共 16 个程序，**144 次运行**均保留；此页选取六个。固定时长的 Larson 不按耗时排名。

耗时使用单调时钟，包含进程启动；峰值 RSS 来自 GNU time 的系统计量，受平台记账限制，与内存报告的驻留页快照不同。

[Raw data](../results/0.0.1/README.md) · [复现](../benchmarking.zh-CN.md)
