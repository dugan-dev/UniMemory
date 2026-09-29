# 独立堆

[目录](../README.zh-CN.md) · [English](heap.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#堆)。

## 范围与生命周期

`Memory::heap(backend)` 创建独立分配组，按需增长，各次分配不保证连续。支持全部通用 Memory 接口；先用 `capabilities(backend).heap` 查询支持情况。Standard 不支持。

## 成员操作

| 方法 | 用途 |
| --- | --- |
| `reset()` | 整体释放，Basic 统计重新开始；失败保留原分配 |
| `collect()` | 回收闲置内存，保留在用分配 |
| `owns(ptr)` | 检查有效分配是否属于当前堆 |
| `statistics()` | 当前堆的可选请求计数 |
| `backend_statistics()` | 可选原生指标；jemalloc 需开启 `config.stats` |

## 使用约定

- reset 或析构前，销毁对象、容器、智能指针及残留的 weak_ptr 控制块；整体释放不调用对象析构
- 分配与释放可并发；collect、reset、Memory 析构必须独占访问，对象读写另需同步
- `owns(nullptr)` 返回 false；其他参数须是同一后端的在用分配起始地址，不能检测任意或悬空指针
- collect 不保证内存占用立即下降，也不保证清空所有线程缓存

```mermaid
flowchart LR
    A[创建堆] --> B[使用对象 / 容器 / 内存块]
    B --> C[销毁对象与所有者]
    C --> D[重置或销毁堆]
```

[栈](stack.zh-CN.md) · [统计](statistics.zh-CN.md) · [生命周期与线程](../compatibility.zh-CN.md)
