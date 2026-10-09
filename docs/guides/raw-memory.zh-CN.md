# 原始内存与对齐

[目录](../README.zh-CN.md) · [English](raw-memory.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#快速开始)。

## 配对与对齐

| 项目 | 约定 |
| --- | --- |
| Memory | 使用分配时的同一个 Memory 释放 |
| 大小 | 调整大小后，使用当前请求的字节数 |
| 对齐 | 使用原对齐值，必须是非零的二次幂 |
| 默认对齐 | `alignof(std::max_align_t)`，满足基本对齐要求 |
| 超对齐对象 | 使用 `alignof(T)`，对象辅助接口会自动设置 |
| 请求大小 | 不必是对齐值的整数倍 |

## 调整大小与所有权

| 操作 | 保证 |
| --- | --- |
| 调整大小 | 保留 `min(old_bytes, new_bytes)` 字节，指针可能变化 |
| 调整失败 | 原指针和内容保持有效 |
| 清零扩容 | 只清零新增请求字节，保留原内容 |
| OwnedBlock | 记录大小与对齐，支持移动所有权，自动释放 |
| OwnedBlock 扩容 | 新增字节未初始化 |
| 字节所有权 | 不调用 C++ 对象析构函数 |

调整大小或替换所有权成功后，丢弃之前保存的 `data()` 指针。已经构造的 C++ 对象应使用对象辅助接口。

## 零大小与错误

| 输入或情况 | 结果 |
| --- | --- |
| `allocate(0)` | `nullptr` |
| `reallocate(nullptr, ..., next)` | 分配内存 |
| 调整为零 | 逻辑释放，返回 `nullptr` |
| `deallocate(nullptr, ...)` | 不执行操作 |
| 无效对齐 | `std::invalid_argument` |
| 分配失败 | `std::bad_alloc` |
| 类型化数量溢出 | `std::length_error` |
| 调整已移走所有权的 OwnedBlock | `std::logic_error` |

Stack 单独释放后仍保留已占用的缓冲区空间。容器适配器为零大小请求提供可配对释放的小块内存。无效指针和错误配对违反约定，不保证能够检测。

[Stack 行为](stack.zh-CN.md) · [所有权](objects.zh-CN.md) · [API](../api-reference.zh-CN.md)
