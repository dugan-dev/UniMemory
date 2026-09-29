# 文档

[UniMemory](../README.zh-CN.md) · [English](README.md) · **简体中文**

[快速开始](../README.zh-CN.md#快速开始)展示常用 API；下列主题说明配置、用法与使用约定。

```mermaid
flowchart LR
    A[接入项目] --> B[分配与对象]
    B --> C[容器与所有权]
    C --> D[堆 / 栈 / 统计]
```

## 1 · 开始使用

| 主题 | 内容 |
| --- | --- |
| [构建与安装](getting-started.zh-CN.md) | CMake 接入与部署 |
| [平台与后端](guides/backends.zh-CN.md) | 平台支持、启用可选后端 |
| [可运行示例](../examples/README.md) | 完整程序 |

## 2 · 日常使用

| 主题 | 内容 |
| --- | --- |
| [原始内存](guides/raw-memory.zh-CN.md) | 分配、释放、对齐与扩容 |
| [对象与数组](guides/objects.zh-CN.md) | 构造、销毁与智能指针 |
| [内存块](guides/raw-memory.zh-CN.md#调整大小与所有权) | 自动释放、调整大小 |
| [标准容器](guides/containers.zh-CN.md) | Allocator、PMR 与嵌套容器 |

## 3 · 内存管理

| 主题 | 内容 |
| --- | --- |
| [堆](guides/heap.zh-CN.md) | 独立管理、整体释放 |
| [栈](guides/stack.zh-CN.md) | 固定缓冲区、标记与回退 |
| [统计](guides/statistics.zh-CN.md) | 用量、峰值与统计范围 |
| [运行时选项](guides/runtime-options.zh-CN.md) | 闲置内存回收延迟 |

## 4 · 查阅

| 主题 | 内容 |
| --- | --- |
| [API](api-reference.zh-CN.md) | 函数、参数与返回值 |
| [生命周期与线程](compatibility.zh-CN.md) | 所有权、并发与动态库 |
| [性能对比](performance.zh-CN.md) | 耗时与内存占用 |
| [测试结果](testing.zh-CN.md) | 验证平台与覆盖范围 |

[后端能力对照](allocator-capabilities.zh-CN.md) · [测量方法](benchmarking.zh-CN.md) · [原始数据](results/0.0.1/README.md) · [上游验证](upstream-validation.md)
