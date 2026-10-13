# 性能报告

历史图表与当前 Windows 本地验证分属不同数据集；尚未跨平台测量本次 Heap 和统计实现。

[UniMemory](../README.zh-CN.md) · [English](performance.md) · **简体中文**

## 概览

第 1–6 节及常用场景图采用历史 CI 数据：2026-10-11 UTC，源码 `8365472eea66997cde5163182aef82d3b94dcc48`，[工作流 38167739252](https://github.com/dugan-dev/UniMemory/actions/runs/38167739252)。每张 SVG 内嵌来源与统计方法。历史 `api_lookup` 每次调用获取旧 Global 单例，`api_cached` 复用计时前取得的引用；均不代表当前直接构造 Global 的接口。

只在同一负载内比较。每张图标明测量来源与统计方法；详细表格展示独立进程测量的波动范围。

| 图例 | 含义 |
| --- | --- |
| 原生接口 | 直接调用分配器 |
| UniMemory 封装 | 关闭统计 |
| 封装＋统计 | 开启统计 |

## 1 · 吞吐量

**看线程增加后每秒能完成多少组分配/释放，越高越好。** 负载为 64 字节跨线程释放。

### Linux x64

![Linux 跨线程吞吐量](images/performance/linux-x64/throughput.zh-CN.svg)

### Windows x64

![Windows 跨线程吞吐量](images/performance/windows-x64/throughput.zh-CN.svg)

### macOS

![macOS 跨线程吞吐量](images/performance/macos-arm64/throughput.zh-CN.svg)

## 2 · 峰值内存

**看同一负载占用多少驻留内存，越低越好。** 包含线程栈、进程和分配器状态。

### Linux x64

![Linux 峰值驻留内存](images/performance/linux-x64/memory.zh-CN.svg)

### Windows x64

![Windows 峰值驻留内存](images/performance/windows-x64/memory.zh-CN.svg)

### macOS

![macOS 峰值驻留内存](images/performance/macos-arm64/memory.zh-CN.svg)

## 3 · 尾延迟

**看不同大小分配的慢请求，越低越好。** P99 表示 99% 的样本不超过该延迟，数值包含时钟开销。

### Linux x64

![Linux 单次分配 P99 延迟](images/performance/linux-x64/latency.zh-CN.svg)

### Windows x64

![Windows 单次分配 P99 延迟](images/performance/windows-x64/latency.zh-CN.svg)

### macOS

![macOS 单次分配 P99 延迟](images/performance/macos-arm64/latency.zh-CN.svg)

## 4 · 统计开销

**看统计开关下的分配延迟，越低越好。** 纹理柱表示开启统计；两种配置独立测量，不直接当作增量。

### Linux x64

![Linux 统计开销](images/performance/linux-x64/statistics.zh-CN.svg)

### Windows x64

![Windows 统计开销](images/performance/windows-x64/statistics.zh-CN.svg)

### macOS

![macOS 统计开销](images/performance/macos-arm64/statistics.zh-CN.svg)

## 5 · 释放后内存

**看分配、保留、循环释放后的内存变化。** 最后阶段已无存活请求，剩余驻留内存可能是缓存，不等于泄漏。

### Linux x64

![Linux 混合生命周期内存](images/performance/linux-x64/retention.zh-CN.svg)

### Windows x64

![Windows 混合生命周期内存](images/performance/windows-x64/retention.zh-CN.svg)

### macOS

![macOS 混合生命周期内存](images/performance/macos-arm64/retention.zh-CN.svg)

## 6 · 常用操作

**看对象、扩容和容器等操作的相对耗时，越低越好。** 同组 Standard 为 1；不同操作之间不比较绝对耗时。

### Linux x64

![Linux 常用操作相对耗时](images/performance/linux-x64/workloads.zh-CN.svg)

### Windows x64

![Windows 常用操作相对耗时](images/performance/windows-x64/workloads.zh-CN.svg)

### macOS

![macOS 常用操作相对耗时](images/performance/macos-arm64/workloads.zh-CN.svg)

## 常用场景

同组 Standard = 1，条形越短越快。

![Windows 与 Linux 耗时对比](images/workload-comparison.png)

## 7 · 当前 Windows 原生/API 耗时与验收

Windows x64 · MSVC 19.44 · C++20 · Release · /O2 /Ob3 · 统计关闭 · 2026-10-13。配置记录 Release IPO=ON、诊断目标 IPO=OFF。每项 10 轮独立进程，交替 Native/API 顺序；计时校准目标 125 ms。保留所有样本。

本次使用直接构造的 Global，构造在计时外。迁移前对照：`69651a1885c3417fa2f0751f04d3e6ecdea6cf25`. 双方使用同一计时内核、SDK 和编译设置；这不是远程 CI 结果。

耗时中位数，单位 ns，越低越好。`std`、`mi`、`je` 为 standard、mimalloc、jemalloc 原生路径，`-unim` 为对应封装。Raw 使用 16 字节对齐并包含释放；其余为完整操作。Vector 包含 reserve(256)、256 次 uint64_t push_back 和销毁。

| 项目 | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 分配＋释放 16 B | 37.32 | 38.21 | 7.83 | 8.66 | 36.77 | 35.55 |
| 分配＋释放 64 B | 38.75 | 38.36 | 8.34 | 9.23 | 34.16 | 33.85 |
| 分配＋释放 256 B | 35.17 | 36.45 | 9.53 | 10.50 | 32.58 | 32.56 |
| 分配＋释放 4 KiB | 36.02 | 36.71 | 28.24 | 29.52 | 41.09 | 39.75 |
| 分配＋释放 64 KiB | 122.42 | 121.44 | 30.84 | 32.68 | 315.83 | 311.45 |
| 清零分配 | 38.59 | 39.53 | 12.29 | 12.60 | 35.84 | 37.13 |
| 重新分配 | 76.01 | 76.82 | 18.42 | 19.12 | 100.69 | 101.53 |
| 重新分配并补零 | 81.96 | 79.62 | 19.03 | 19.21 | 107.53 | 106.79 |
| Unique 对象，64 B | 38.51 | 37.66 | 7.72 | 7.34 | 34.31 | 33.40 |
| 对象数组 | 54.86 | 53.76 | 24.24 | 25.03 | 47.75 | 47.68 |
| Shared 对象 | 69.86 | 71.21 | 37.62 | 37.95 | 69.75 | 69.70 |
| Vector，256 个 64 位整数 | 244.67 | 302.94 | 279.84 | 265.72 | 310.14 | 311.26 |
| PMR Vector，256 个 64 位整数 | 825.68 | 695.57 | 717.26 | 678.86 | 787.95 | 829.45 |
| OwnedBlock，64 B | 40.33 | 41.03 | 6.22 | 7.01 | 36.38 | 36.18 |

### 相对迁移前

| 后端 | 更快项目 / 总项目 | 等权几何平均速度提升 |
| --- | ---: | ---: |
| standard | 8 / 14 | 2.1% |
| mimalloc | 13 / 14 | 6.6% |
| jemalloc | 7 / 14 | -2.9% |

42 项中 28 项实际耗时优于迁移前；微小差异可能属于测量波动。

### 仍低于 90% 原生门槛

门槛同时检查配对中位数及算术平均耗时比；39/42 项通过。全部失败结果保留，门槛未变。

| 后端 | 项目 | 配对中位数 | 均值耗时比 |
| --- | --- | ---: | ---: |
| standard | Vector，256 个 64 位整数 | 81.18% | 79.94% |
| mimalloc | 分配＋释放 16 B | 88.78% | 89.56% |
| mimalloc | OwnedBlock，64 B | 88.23% | 88.55% |

### 正确性

本次本地迁移验证：5,337 项源码 CTest 和 54 项安装消费者 CTest 通过。

来源：`docs/results/windows-local-comparison.json`，测量对象是 2026-10-13 本地工作树，未记录固定的当前源码提交号。单 CPU 亲和；构造成本不计入本表。

## 8 · Linux 原生应用：耗时与峰值内存

历史 CI 数据：2026-10-11 UTC，源码 `8365472eea66997cde5163182aef82d3b94dcc48`，同一工作流。16 个独立程序 × 3 个分配器，每项 10 个新进程，线程负载使用 4 个工作线程。mimalloc-bench 固定版本为 `ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86`。进程级分配器替换衡量原生分配器行为，不衡量 UniMemory 封装。

下表摘录三个负载，每格为算术平均 [最小值, 最大值]。耗时单位 s，含进程启动；峰值 RSS 单位 MiB，来自 GNU time，与前文阶段内存快照不同。两者越低越好。

| 程序 / 指标 | glibc | mimalloc | jemalloc |
| --- | --- | --- | --- |
| cfrac / 耗时 s | 3.679 [3.672, 3.725] | 3.339 [3.323, 3.477] | 3.433 [3.422, 3.522] |
| cfrac / 峰值 MiB | 3.26 [3.12, 3.36] | 7.01 [6.96, 7.13] | 10.72 [10.63, 10.82] |
| malloc-large / 耗时 s | 1.268 [1.218, 1.369] | 1.175 [1.118, 1.268] | 3.474 [3.424, 3.625] |
| malloc-large / 峰值 MiB | 522.03 [521.93, 522.18] | 625.18 [625.16, 625.24] | 448.43 [446.62, 450.64] |
| glibc-simple / 耗时 s | 2.219 [2.219, 2.220] | 1.619 [1.617, 1.620] | 2.024 [2.018, 2.069] |
| glibc-simple / 峰值 MiB | 1.97 [1.97, 1.97] | 6.30 [6.27, 6.34] | 14.78 [14.38, 16.64] |

larson、larson-sized、xmalloc-test 按固定持续时间运行，耗时不参与比较。原始记录保留于 `docs/results/current/native-applications/`；本摘录不代表全部 16 个程序的排名。

## 数据与方法

### 历史 CI 环境（2026-10-11 UTC）

| 平台 | CPU / 逻辑核 | 编译器 | 驻留内存 |
| --- | --- | --- | --- |
| Linux x64 | AMD EPYC 9V74 / 4 | GCC 13.3.0 | smaps_rollup RSS |
| Windows x64 | Intel Family 6 Model 207 / 4 | MSVC 19.44.35229.0 | 工作集 |
| macOS ARM64 | Apple M1 (Virtual) / 3 | AppleClang 17.0.0 | Mach resident_size |

同平台后端与统计 OFF/ON 共用受控主机和 SDK。Release；先预热，通过三轮持续校准选取操作数，交替执行顺序，每项至少 10 个独立进程。Native/API 同配置匹配操作数，不同配置可能采用不同有界操作数。过量线程扩展最多 32 轮；超过主机核数的点包含调度与同步压力。

吞吐量、内存和常用操作图采用进程试验算术平均；尾延迟图采用各进程 P99 的算术平均，包含时钟开销。内存逐轮减去基线后汇总。统计 OFF/ON 独立运行。保留全部样本，汇总包含均值、中位数、最小/最大值、样本标准差、CV、bootstrap 95% 区间；微小差异或高波动不能视为确定收益。

历史环境与数据保留于 `docs/results/current/<平台>/<后端>/<OFF|ON>/`，SVG 元数据记录具体文件、协议及源码。仓库保留进程试验，逐操作轨迹和构建证据按工作流制品 90 天策略保留。旧数据不能外推到当前 Heap、统计或 Global 构造接口，也不能跨机器排名。

### 本地复现与验收

```sh
cmake -S . -B build/performance -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_PERFORMANCE_TESTS=ON
cmake --build build/performance --config Release --target UniMemoryPerformanceAcceptance
```

每个后端/统计配置独立构建。性能目标串行计时，与 CTest 正确性检查分开。默认校准 250 ms，第 7 节实测为 125 ms。至少 10 轮交替进程试验同时要求配对中位数和算术平均耗时比达到 90% Native；门槛只能收紧。只读 `--profile` 校验编译设置、统计和检查；拒绝 sanitizer/coverage 构建。Native/API 必须采用相同 SDK、CRT、优化和 IPO 策略；IPO 默认关闭，不强加给安装消费者。

在空闲机器运行，每次新建输出目录。Windows/Linux 要求 CPU 亲和；macOS 不固定 CPU 并记录限制。校准不足保留证据并失败，不删除异常值。正确性验收覆盖独立模块、组合使用和安装消费者；第 7 节的 5,337/54 通过数为已有本地迁移记录，不是本次文档编辑重跑的测试。
