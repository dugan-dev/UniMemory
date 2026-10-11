# 原生程序

[性能](../performance.zh-CN.md) · [English](applications.md) · **简体中文**

来自 mimalloc-bench 的独立 Linux 程序，进程级替换分配器。**比较原生分配器，不测 UniMemory 封装成本。**

<!-- performance-source -->
2026-10-11 · [81cfacd](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240) · [CSV](../results/current/native-applications/README.md)
<!-- /performance-source -->

[固定版本的基准集合](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 个程序 × 3 个分配器 × 3 个进程，共 144 次成功运行。每行取三次中位数；系统分配器为 glibc，线程场景使用 4 个工作线程。

## 耗时

**秒，越低越好。** Larson、larson-sized 和 xmalloc-test 按配置时长运行，不按耗时排名；其运行时间保留在 CSV 中。

<!-- performance-table:applications-elapsed -->
| 程序 | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.672 | 3.322 | 3.422 |
| espresso | 4.071 | 3.622 | 3.673 |
| barnes | 2.319 | 2.320 | 2.318 |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 4.224 | 3.170 | 3.222 |
| cache-scratch | 0.568 | 0.616 | 0.565 |
| cache-thrash | 0.565 | 0.619 | 0.566 |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 | 0.004 | 0.008 |
| malloc-large | 1.168 | 1.067 | 3.323 |
| mstress | 0.216 | 0.166 | 0.365 |
| mleak | 0.032 | 0.064 | 0.064 |
| rptest | 16.042 | 16.043 | 16.043 |
| glibc-simple | 2.219 | 1.667 | 2.018 |
| glibc-thread | 2.022 | 2.023 | 2.020 |
<!-- /performance-table -->

## 峰值内存

**峰值 RSS，MiB，越低越好。**

<!-- performance-table:applications-memory -->
| 程序 | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.29 | 7.07 | 10.73 |
| espresso | 2.63 | 12.62 | 10.80 |
| barnes | 60.70 | 67.15 | 75.14 |
| larson | 41.92 | 64.85 | 70.51 |
| larson-sized | 39.83 | 64.77 | 72.31 |
| alloc-test | 14.77 | 21.42 | 30.90 |
| cache-scratch | 3.89 | 9.23 | 10.64 |
| cache-thrash | 3.88 | 9.16 | 10.64 |
| xmalloc-test | 55.04 | 66.17 | 130.69 |
| malloc-large-old | 3.90 | 8.30 | 10.52 |
| malloc-large | 521.96 | 625.15 | 446.62 |
| mstress | 67.80 | 156.39 | 67.08 |
| mleak | 1.91 | 6.27 | 26.54 |
| rptest | 13.53 | 29.11 | 40.07 |
| glibc-simple | 1.85 | 6.28 | 14.66 |
| glibc-thread | 2.35 | 16.45 | 26.34 |
<!-- /performance-table -->

## 如何解读

不同负载对分配器的结果差异很大；更快不一定更省内存。按实际对象尺寸、线程与生命周期选择后端。

耗时使用单调时钟，包含进程启动；峰值 RSS 来自 GNU time 的系统计量，与内存报告的阶段快照不同。

[全部 144 次原始数据](../results/current/native-applications/README.md) · [复现](../benchmarking.zh-CN.md)
