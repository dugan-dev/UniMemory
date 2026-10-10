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
| [配置迁移](migration.zh-CN.md) | 固定后端、统计和检查策略 |
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
| [远程验收](remote-validation.md) | 原生编译器矩阵、错误检测与图表自动更新 |

[后端能力对照](allocator-capabilities.zh-CN.md) · [测量方法](benchmarking.zh-CN.md) · [原始数据](results/0.0.1/README.md) · [上游验证](upstream-validation.md)

## 5 · 验证与维护

历史修复验证日期为 **2026-10-08 至 2026-10-09**；这些旧编译库计数不代表当前纯头文件配置。

| 验证 | 已记录结果 |
| --- | --- |
| Windows：三种 Backend + 示例 | Release、Debug 各 **1666/1666** |
| Linux：三种 Backend + 示例 | **1667/1667**，安装消费者 **7/7** |
| macOS：三种 Backend + 示例 | [GitHub CI 通过](https://github.com/dugan-dev/UniMemory/actions/runs/37871859097) |
| ASan / UBSan，Standard 与 Stack | **715/715**，启用泄漏检测 |
| ThreadSanitizer，Standard 与 Stack | [发布验证通过](https://github.com/dugan-dev/UniMemory/actions/runs/37871859066) |

共享库配置在 Windows 通过 **1664/1664**、Linux 通过 **1666/1666**；六项本地配置的安装消费者均通过 **7/7**。覆盖常规使用、类型化存储、独立公共头文件、包查找、边界、异常和并发场景。各 CI 链接只验证其记录的修订。[完整测试报告](testing.zh-CN.md)

此处介绍的纯头文件固定配置位于 `dev`；原稳定实现保留在 `main`。
通过 [dev 源码 ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip)
提供下载，不使用 tag 或 Release。库版本仍为 **0.0.1**，以源码修订标识交付，
头文件、生成配置与 SDK 库应来自同一修订。[当前性能报告](performance.zh-CN.md#当前远程报告)
记录各自测量修订；九月数据作为历史报告保留。

[贡献指南](../CONTRIBUTING.md) · [安全政策](../SECURITY.md) · [修复审查](review-2026-10-08.md)

文档发布前运行：

```sh
python tools/check-docs.py
```
