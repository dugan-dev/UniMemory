# 分配耗时

[Performance](../performance.zh-CN.md) · [English](latency.md) · **简体中文**

**纳秒/完整操作；越低越快。** 每个场景取三进程中位数；进程内预热后重复七次。Global，统计关闭。

## 1 · Object / Container / Block

### Windows / MSVC

| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 46.4 | 12.2 | 36.0 |
| make_shared, 64 B | 63.1 | 23.7 | 56.5 |
| vector, 32 integers | 590.3 | 237.4 | 488.5 |
| PMR vector, 32 integers | 580.5 | 253.6 | 494.7 |
| Zeroed Block, 4 KiB | 81.3 | 52.1 | 70.7 |
| Block resize, 4 to 8 KiB | 168.5 | 139.8 | 149.4 |
| Mixed lifetimes, 4-16 KiB | 152.0 | 44.9 | 138.5 |
| Cross-thread free, 8 threads | 105.4 | 59.8 | 166.6 |

### Linux / GCC / WSL

| Workload | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- |
| make_unique, 64 B | 18.8 | 10.6 | 20.0 |
| make_shared, 64 B | 20.8 | 13.3 | 27.3 |
| vector, 32 integers | 153.2 | 111.5 | 190.4 |
| PMR vector, 32 integers | 171.8 | 131.6 | 207.0 |
| Zeroed Block, 4 KiB | 72.0 | 59.9 | 65.1 |
| Block resize, 4 to 8 KiB | 119.1 | 94.6 | 111.4 |
| Mixed lifetimes, 4-16 KiB | 134.6 | 35.0 | 93.1 |
| Cross-thread free, 8 threads | 139.3 | 53.5 | 151.4 |

Object 包含创建与销毁；vector 包含增长与销毁；resize 包含分配、扩容和释放。只能在同一行内比较。

![Backend workload comparison](../images/workload-comparison.png)

每行 Standard = 1；越短越快。

## 2 · Native / API / Basic

一次分配/释放，16 字节对齐。Native 直接调用后端；API 关闭统计；Basic 开启请求计数。每进程九次 × 200000 操作，三个进程。

### Windows

| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 64 | Standard | 42.3 | 44.4 | 72.3 |
| 64 | mimalloc | 7.1 | 11.2 | 33.5 |
| 64 | jemalloc | 36.2 | 38.5 | 65.6 |
| 4096 | Standard | 44.7 | 45.4 | 70.8 |
| 4096 | mimalloc | 28.2 | 32.5 | 54.3 |
| 4096 | jemalloc | 42.6 | 43.7 | 71.5 |

### Linux / WSL

| Bytes | Backend | Native | API | Basic |
| --- | --- | --- | --- | --- |
| 64 | Standard | 15.9 | 21.0 | 44.1 |
| 64 | mimalloc | 9.0 | 12.2 | 34.3 |
| 64 | jemalloc | 18.1 | 24.1 | 44.1 |
| 4096 | Standard | 28.1 | 34.5 | 54.6 |
| 4096 | mimalloc | 29.9 | 31.6 | 58.2 |
| 4096 | jemalloc | 21.0 | 29.5 | 47.9 |

关闭统计没有计数原子更新，仍有对齐检查和后端调用成本。Basic 的原子计数有实际开销。Native Standard 使用 throwing aligned new，API 使用 nothrow aligned new 并转换失败；成功请求的大小及对齐一致。

## 3 · 跨线程释放

一条线程分配，工作线程释放。线程创建在计时前，等待完成计时。

| Platform | Workers | Standard | mimalloc | jemalloc |
| --- | --- | --- | --- | --- |
| Windows | 2 | 118.7 | 53.8 | 81.5 |
| Windows | 4 | 101.2 | 49.5 | 100.1 |
| Windows | 8 | 105.4 | 59.8 | 166.6 |
| Linux | 2 | 125.9 | 48.4 | 54.4 |
| Linux | 4 | 124.8 | 42.5 | 76.3 |
| Linux | 8 | 139.3 | 53.5 | 151.4 |

## 4 · Stack

预先准备 Buffer，一次 mark/allocate/rewind。只写入一个字节，不是等价的 Heap 分配场景。

| Platform | Bytes | ns/cycle |
| --- | --- | --- |
| Windows | 64 | 3.49 |
| Windows | 4096 | 3.58 |
| Linux | 64 | 4.13 |
| Linux | 4096 | 4.15 |

未绑定 CPU；小差异可能是噪声。原始数据保留三进程及进程内最小/中位/最大值，不提供 p95/p99 结论。

[测量方法](../benchmarking.zh-CN.md) · [Raw data](../results/0.0.1/README.md) · [Memory](memory.zh-CN.md)
