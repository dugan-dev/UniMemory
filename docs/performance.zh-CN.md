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

## 7 · 最新三平台原生／封装对比

2026-10-13 · 测量版本 `4f98078` · Actions `38372589028`。三平台正确性全部通过；核心性能门槛通过 106/126 项，Windows 与 macOS 未全部达标。

Global 核心接口，Release、统计关闭，10 轮配对测量。表格为 ns 中位耗时，越低越快。FAIL 表示配对中位数或平均耗时比低于原生的 90%。不能仅用表中两个中位数相除判断验收。

### Windows · MSVC 19.44.35229.0

| 操作 | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 分配释放 16 B | 32.89 | 33.40 | 5.15 | 5.22 FAIL | 35.14 | 42.78 FAIL |
| 分配释放 64 B | 32.87 | 32.89 | 5.12 | 5.17 | 37.30 | 41.78 FAIL |
| 分配释放 256 B | 33.30 | 33.61 | 5.79 | 6.00 | 35.22 | 41.29 FAIL |
| 分配释放 4 KiB | 33.36 | 33.61 | 20.68 | 20.94 | 35.44 | 39.90 FAIL |
| 分配释放 64 KiB | 104.63 | 106.53 | 22.86 | 23.02 | 218.03 | 221.29 |
| 清零分配 | 35.12 | 35.96 | 10.51 | 11.18 | 38.08 | 45.54 FAIL |
| 扩容 | 70.01 | 69.97 | 19.31 | 19.22 | 89.56 | 90.34 |
| 扩容并清零 | 70.40 | 71.12 | 19.28 | 19.81 | 90.55 | 91.05 |
| Unique 对象 | 35.81 | 34.95 | 7.72 | 7.31 | 33.77 | 34.00 |
| 对象数组 | 49.11 | 48.84 | 22.07 | 21.87 | 46.55 | 47.88 |
| Shared 对象 | 40.86 | 42.82 | 12.62 | 13.69 | 56.13 | 58.42 |
| Vector | 240.58 | 242.15 | 226.01 | 229.18 | 250.63 | 240.97 |
| PMR Vector | 866.72 | 868.49 | 848.51 | 849.24 | 868.69 | 873.72 |
| OwnedBlock | 34.02 | 34.30 | 4.81 | 5.34 | 46.13 | 49.94 |

36/42 项达标。

### Linux · GNU 13.3.0

| 操作 | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 分配释放 16 B | 11.38 | 11.07 | 7.96 | 7.99 | 15.35 | 15.35 |
| 分配释放 64 B | 11.37 | 11.05 | 8.25 | 8.21 | 15.36 | 15.37 |
| 分配释放 256 B | 11.37 | 11.07 | 8.83 | 8.83 | 15.60 | 15.60 |
| 分配释放 4 KiB | 23.08 | 22.64 | 27.44 | 28.12 | 18.91 | 18.87 |
| 分配释放 64 KiB | 25.47 | 25.26 | 29.31 | 29.50 | 319.82 | 320.92 |
| 清零分配 | 12.43 | 12.50 | 11.21 | 10.91 | 19.31 | 18.85 |
| 扩容 | 24.61 | 25.36 | 20.91 | 21.84 | 46.99 | 47.21 |
| 扩容并清零 | 24.21 | 24.59 | 22.50 | 22.23 | 48.10 | 48.09 |
| Unique 对象 | 27.08 | 11.17 | 20.43 | 7.54 | 26.30 | 13.39 |
| 对象数组 | 69.87 | 25.18 | 33.54 | 25.59 | 38.61 | 24.36 |
| Shared 对象 | 15.70 | 16.55 | 11.98 | 12.25 | 21.22 | 21.96 |
| Vector | 200.51 | 122.71 | 134.42 | 134.37 | 118.84 | 120.72 |
| PMR Vector | 135.56 | 139.58 | 146.14 | 143.74 | 128.09 | 129.40 |
| OwnedBlock | 13.46 | 12.11 | 10.44 | 9.51 | 17.82 | 17.57 |

42/42 项达标。

### macOS · AppleClang 17.0.0.17000013

| 操作 | std | std-unim | mi | mi-unim | je | je-unim |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 分配释放 16 B | 17.72 | 21.13 FAIL | 5.18 | 5.74 FAIL | 15.75 | 16.24 |
| 分配释放 64 B | 22.92 | 26.21 FAIL | 5.43 | 5.99 | 17.31 | 17.76 |
| 分配释放 256 B | 22.27 | 23.36 | 7.03 | 7.65 | 17.98 | 19.67 |
| 分配释放 4 KiB | 27.86 | 27.34 | 15.95 | 15.13 | 24.00 | 24.83 |
| 分配释放 64 KiB | 29.30 | 27.23 | 16.64 | 16.69 | 207.79 | 208.90 |
| 清零分配 | 25.45 | 27.42 | 7.27 | 8.73 FAIL | 21.63 | 25.80 FAIL |
| 扩容 | 57.81 | 62.08 | 14.18 | 15.84 FAIL | 49.30 | 48.07 |
| 扩容并清零 | 56.49 | 61.08 | 16.21 | 16.38 | 49.39 | 53.11 |
| Unique 对象 | 29.75 | 34.80 FAIL | 6.21 | 7.52 FAIL | 17.08 | 19.57 FAIL |
| 对象数组 | 69.44 | 80.61 FAIL | 21.11 | 29.88 FAIL | 36.83 | 49.20 FAIL |
| Shared 对象 | 33.71 | 32.67 | 21.16 | 20.99 | 35.10 | 33.19 |
| Vector | 145.62 | 161.06 FAIL | 737.06 | 134.92 | 824.46 | 177.12 |
| PMR Vector | 160.80 | 158.84 | 142.00 | 154.77 | 172.47 | 180.88 |
| OwnedBlock | 20.78 | 21.99 | 5.31 | 5.70 | 19.34 | 21.82 FAIL |

28/42 项达标。

数据保存在 `docs/results/latest-validation.json`，原图数据为 `docs/results/latest-overview.json`。Linux/macOS 的部分对象与容器出现数倍差距，尚不能直接归因于分配器本身，需要结合基准和生成代码复核。历史图和应用数据按各自标注的来源阅读。

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
