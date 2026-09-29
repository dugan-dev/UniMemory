# 文档

[UniMemory](../README.zh-CN.md) · [English](README.md) · **简体中文**

文档涵盖基础分配、可选功能、API 约定与验证结果。

```mermaid
flowchart LR
    A[Start] --> B[Object / Container / Block]
    B --> C[Optional features]
    C --> D[API / Benchmarks / Tests]
```

## 1 · 开始

| 主题 | 用途 |
| --- | --- |
| [编译与安装](getting-started.zh-CN.md) | 构建库，接入项目 |
| [API 概览](api-reference.zh-CN.md) | Global、Heap、Stack 的创建方式 |

## 2 · 日常使用

| 主题 | 用途 |
| --- | --- |
| [Object / Array](guides/objects.zh-CN.md) | 创建、销毁、Smart Pointer |
| [Container](guides/containers.zh-CN.md) | std Container 与 PMR |
| [Block](guides/raw-memory.zh-CN.md) | 对齐、扩容、自动释放 |

## 3 · 按需使用

| 主题 | 用途 |
| --- | --- |
| [Backend](guides/backends.zh-CN.md) | 配置、平台、依赖 |
| [Heap](guides/heap.zh-CN.md) | 独立管理一组分配 |
| [Stack](guides/stack.zh-CN.md) | 固定 Buffer 内的临时分配 |
| [Statistics](guides/statistics.zh-CN.md) | 请求计数与 Backend 详情 |
| [Runtime options](guides/runtime-options.zh-CN.md) | 控制闲置内存回收延迟 |

## 4 · 查阅

| 主题 | 用途 |
| --- | --- |
| [API](api-reference.zh-CN.md) | 函数与约束 |
| [Compatibility](compatibility.zh-CN.md) | 生命周期、线程、配对规则 |
| [Performance](performance.zh-CN.md) | 耗时、内存、原生程序对比 |
| [Tests](testing.zh-CN.md) | 测试范围与结果 |

## 更多资料

[Examples](../examples/README.md) · [Benchmark method](benchmarking.zh-CN.md) · [Raw data](results/0.0.1/README.md) · [Upstream tests](upstream-validation.md)

[Capability comparison](allocator-capabilities.zh-CN.md) · [mimalloc](backends/mimalloc.zh-CN.md) · [jemalloc](backends/jemalloc.zh-CN.md) · [TCMalloc](backends/tcmalloc.zh-CN.md)
