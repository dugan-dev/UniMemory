# 性能测量

[目录](README.zh-CN.md) · [English](benchmarking.md) · **简体中文** · [结果](performance.zh-CN.md)

比较分配器时，使用同一设备、编译器、运行库和优化选项。不同 CPU 的结果不能用于给后端排名。

## 构建

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=OFF -DUNIMEMORY_BUILD_BENCHMARKS=ON
cmake --build build/bench --config Release
```

后端与统计组合分别构建，可选 SDK 接入见[后端指南](guides/backends.zh-CN.md)。

## 测量范围

| 维度 | 内容 |
| --- | --- |
| 原始分配 | 16–65536 B，相同大小、对齐的原生与 API 调用 |
| 对象与容器 | 构造/析构、数组、vector、PMR 和所有权 |
| 并发 | 同线程与跨线程分配释放，1–16 个工作线程 |
| 内存 | 在用请求、驻留内存、释放与回收 |
| 原生应用 | 固定版本 mimalloc-bench 中的 16 个程序 |

每个平台在同一个 runner 上构建所有后端，预热、校准足够的计时长度、交替执行顺序，并保留至少十轮独立进程测量。配对的原生/API 路径使用相同操作次数和 SDK 二进制。

每个配置进行三次持续校准试跑。线程超出 runner 容量时，扩展测试最多 32 轮，并标明较短的计时间隔与校准限制。线程扩展包含同步开销，超量线程属于压力场景。

报告展示算术平均值、中位数、最小值、最大值、样本标准差、变异系数和 bootstrap 95% 区间。保留全部样本；波动大时直接标明，不删除慢样本或把波动当作优化收益。内存阶段先减去各轮基线，再做统计。

每轮进程数据与统计汇总无损保存在仓库。逐次操作轨迹与构建证据保存在关联的工作流产物中，保留 90 天；发布前核验轨迹。

## 与 mimalloc 官方测试的关系

[mimalloc 官方报告](https://github.com/microsoft/mimalloc#performance)使用十轮平均值，覆盖实际应用与合成负载。[mimalloc-bench](https://github.com/daanx/mimalloc-bench)提供本库使用的原生程序子集。

本库还测量 C++ 对象、容器和上下文处理；Standard 使用 C++ 分配接口，与官方 glibc malloc 基线不同。进程级替换分配器测量的是原生行为，不是封装开销。没有分配器在所有场景下都最快。

[数据与环境](results/current/README.md) · [验证](testing.zh-CN.md)
