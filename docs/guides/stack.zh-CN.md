# 栈式分配

[目录](../README.zh-CN.md) · [English](stack.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#快速开始)。

## 缓冲区生命周期

`Memory::stack(buffer)` 在现有缓冲区中分配，支持全部通用 Memory 接口。缓冲区须可写、固定容量、比 Memory 存活更久，仅限单线程。它不是线程的调用栈。

## 回收空间

| 操作 | 行为 |
| --- | --- |
| `allocate()` | 推进位置，包含对齐填充 |
| `deallocate()` | 不回收 Buffer 空间，不调用 Object 析构 |
| Owner 析构 | 按需调用 Object 析构，不回收 Buffer 空间 |
| `mark()` | 保存当前位置 |
| `rewind(mark)` | 回退位置，所有旧标记失效 |
| `reset()` | 回到起点，所有旧标记失效 |
| `used()` / `capacity()` | 已占用字节 / 总字节 |

回退、reset 前先销毁受影响的 Owner 和 Container；回退不执行 Object 析构。
其他实例或失效的标记抛 `invalid_argument`。Mark 不能超过 Memory 生命周期；不要同时管理重叠 Buffer。

回退内层 Mark 后，外层 Mark 也失效，不能继续用旧标记逐层回退。

## 重分配与失败

| 场景 | 结果 |
| --- | --- |
| 容量不足 | `bad_alloc`，位置不变，不回退到 Heap |
| 块结束于当前位置且空间足够 | 原位扩展 |
| 其他扩容 | 同一 Buffer 内另分配并复制 |
| 缩容或单块释放 | 保留已占用空间 |
| 重分配失败 | 原指针、数据及位置保持有效 |

Stack 没有 Backend 或原生统计。`used()` 表示 Buffer 占用，包含填充和单块释放后保留的空间。

[标准容器](containers.zh-CN.md) · [API](../api-reference.zh-CN.md)
