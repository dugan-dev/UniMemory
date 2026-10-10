# 性能基准

[目录](README.zh-CN.md) · [English](benchmarking.md) · **简体中文** · [结果](performance.zh-CN.md)

按实际负载测量。[mimalloc-bench](https://github.com/daanx/mimalloc-bench)提供应用与合成场景；它是基准集合，不是行业认证或统一评分标准。

## 1 · 构建

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BUILD_BENCHMARKS=ON
cmake --build build/bench --config Release
```

Standard 无分配器依赖。可选 Backend 需要[配置选项及依赖](guides/backends.zh-CN.md)。

## 2 · 测量

Linux / macOS：

```sh
python3 tools/run-benchmarks.py build/bench/UniMemoryDeliveryBenchmark build/results --backends standard
python3 tools/run-sweep.py build/bench/UniMemoryBenchmark build/results
```

Windows 使用 `python`，可执行文件在 `build/bench/Release/`，后缀 `.exe`。启用三种 Backend 后使用 `--backends standard mimalloc jemalloc`；脚本默认期待三种 Backend。

| 测量 | 负载 | 输出 |
| --- | --- | --- |
| Native / API / Basic | 16–65536 B，16 字节对齐，分配/释放 | latency.csv |
| 内存占用 | 16384 × 4096 B，全部写入，释放/collect | footprint.csv |
| 混合尺寸 | 16384 个 Block，少量保留，八轮分配/释放 | pressure.csv |
| Heap 操作 | owns、collect、64 B 分配 + reset | heap.csv |
| API 扫描 | Bytes、Object、Array、Container、所有权、跨线程、诊断、Stack | full.csv |
| 环境 | 原生版本、类型大小、源码/程序 SHA-256 | environment.json、sweep-environment.json |

Global 的统计模式在首次获取前固定，因此 Disabled、Basic 分别运行。每轮 Heap 和 Stack 不重复测量。

## 3 · 测量方法

| 探针 | 重复次数 |
| --- | --- |
| 基础耗时 | 预热；进程内 9 × 200000 次操作；3 个独立进程 |
| 完整扫描 | 预热；7 次重复，随机 Backend 顺序；3 个独立进程 |
| 内存占用 | 3 个独立进程，写入内存页，检查内容 |

一次操作表示完整场景：vector 增长并销毁 32 个元素；resize 包含分配、扩容、释放；跨线程测试在一条线程分配，由 2/4/8 条工作线程释放。线程创建不计时，等待完成计时。普通分配不计 Heap 创建；reset 探针计入替换 Heap 的准备。

混合探针使用固定种子 `0xC0FFEE`、十种 17–8193 B 尺寸、16 字节对齐和 1024 个长期 Block。检查内容、对齐、计数、原生可用容量。诊断不计入分配循环。RSS 包含缓存、元数据和进程状态；释放后保留内存不等于碎片率。

原生统计场景计时包含能力不支持时返回空值。先查询实例能力；返回空值的调用不等价于获取完整原生指标。

## 4 · 后端隔离

mimalloc 使用 `MI_OVERRIDE=OFF`，Windows 同时 `MI_WIN_REDIRECT=OFF`。Windows 子进程还设置并核对 `MIMALLOC_DISABLE_REDIRECT=1`。

Unix jemalloc 使用 `--with-jemalloc-prefix=je_ --disable-cxx`。仅前缀仍可能替换全局 new/delete；CMake 检查符号，基准检查 Unix 全局 new 的来源。Native Standard 使用 throwing aligned new，API 使用 nothrow aligned new 并转换失败；成功请求的大小/对齐一致，失败路径不同。

## 5 · 复现表格和图表

```sh
python3 tools/summarize-benchmarks.py docs/results/0.0.1 --output build/comparisons.json
```

汇总检查已发布的两平台、三 Backend 矩阵。Python 脚本仅依赖标准库。

可选图表导出使用 Windows .NET Framework：

```powershell
powershell -NoProfile -File tools/render-benchmarks.ps1 -InputJson build/comparisons.json -OutputDirectory build/charts
```

上述批量计时不能推断逐操作 p95/p99。未绑定 CPU；小差异可能是噪声。
这些短时测试不能证明 NUMA 扩展性、长期碎片或移动设备性能。

## 6 · 当前自动报告

代码或配置变更触发[性能流程](../.github/workflows/performance.yml)，在 Linux x64、
Windows x64、macOS ARM64 构建三种后端。现有探针之外，增加 Native/API/Basic
扩展性与逐操作延迟采样；完整 API 场景校验通过后生成并发布 SVG。

| 每平台测量 | 规模 |
| --- | --- |
| 扩展性 | 810 项：三轮、三后端、三调用路径、两种释放方式、三种大小、五种线程数 |
| 尾延迟 | 135 项，每项 8,192 组分配/释放样本；单独记录时钟开销 |
| API 场景 | 每轮 1,001 项，共三轮 |
| 外部原生应用 | Linux：16 个程序、三种分配器、三轮 |

扩展性测量每线程执行 4,096 组操作，分为八批、每批 512 个；各路径请求与数据触碰
一致，最大存活负载 512 MiB。尾延迟来自包含时钟开销的逐操作间隔；RSS 包含进程
和分配器状态。应在同一测量环境内比较负载。

[报告](performance.zh-CN.md#当前远程报告) · [协议与自动发布](remote-validation.md#performance-and-charts)

[原始数据](results/0.0.1/README.md) · [原生测试与应用](upstream-validation.md)
