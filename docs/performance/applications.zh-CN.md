# 原生程序

[性能](../performance.zh-CN.md) · [English](applications.md) · **简体中文**

来自 mimalloc-bench 的独立 Linux 程序，进程级替换分配器。**比较原生分配器，不测 UniMemory 封装成本。**

<!-- performance-source -->
2026-10-11 · [e758dc2](https://github.com/dugan-dev/UniMemory/actions/runs/38101997387) · [CSV](../results/current/native-applications/README.md)
<!-- /performance-source -->

[固定版本的基准集合](https://github.com/daanx/mimalloc-bench/tree/ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86)

16 个程序 × 3 个分配器 × 3 个进程，共 144 次成功运行。每行取三次中位数；系统分配器为 glibc，线程场景使用 4 个工作线程。

## 耗时

**秒，越低越好。** Larson、larson-sized 和 xmalloc-test 按配置时长运行，不按耗时排名；其运行时间保留在 CSV 中。

<!-- performance-table:applications-elapsed -->
| 程序 | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.672 | 3.323 | 3.422 |
| espresso | 4.073 | 3.622 | 3.674 |
| barnes | 2.420 | 2.318 | 2.370 |
| larson | — | — | — |
| larson-sized | — | — | — |
| alloc-test | 4.224 | 3.277 | 3.226 |
| cache-scratch | 0.566 | 0.618 | 0.566 |
| cache-thrash | 0.568 | 0.616 | 0.566 |
| xmalloc-test | — | — | — |
| malloc-large-old | 0.004 | 0.004 | 0.008 |
| malloc-large | 1.218 | 1.118 | 3.424 |
| mstress | 0.214 | 0.216 | 0.416 |
| mleak | 0.032 | 0.064 | 0.064 |
| rptest | 16.046 | 16.045 | 16.045 |
| glibc-simple | 2.218 | 1.617 | 2.019 |
| glibc-thread | 2.023 | 2.022 | 2.023 |
<!-- /performance-table -->

## 峰值内存

**峰值 RSS，MiB，越低越好。**

<!-- performance-table:applications-memory -->
| 程序 | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac | 3.32 | 7.06 | 10.71 |
| espresso | 2.69 | 12.56 | 10.80 |
| barnes | 60.67 | 67.23 | 73.30 |
| larson | 41.73 | 62.73 | 69.45 |
| larson-sized | 39.25 | 62.72 | 71.75 |
| alloc-test | 14.82 | 21.33 | 29.21 |
| cache-scratch | 3.94 | 9.12 | 10.63 |
| cache-thrash | 3.87 | 9.23 | 10.63 |
| xmalloc-test | 58.69 | 66.17 | 120.62 |
| malloc-large-old | 3.90 | 8.30 | 10.62 |
| malloc-large | 522.01 | 625.16 | 448.61 |
| mstress | 68.27 | 126.40 | 67.15 |
| mleak | 1.91 | 6.21 | 18.52 |
| rptest | 13.08 | 28.98 | 39.34 |
| glibc-simple | 1.85 | 6.27 | 14.64 |
| glibc-thread | 2.35 | 16.39 | 26.38 |
<!-- /performance-table -->

## 如何解读

不同负载对分配器的结果差异很大；更快不一定更省内存。按实际对象尺寸、线程与生命周期选择后端。

耗时使用单调时钟，包含进程启动；峰值 RSS 来自 GNU time 的系统计量，与内存报告的阶段快照不同。

[全部 144 次原始数据](../results/current/native-applications/README.md) · [复现](../benchmarking.zh-CN.md)
