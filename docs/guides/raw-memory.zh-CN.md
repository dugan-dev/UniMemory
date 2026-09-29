# Block

[目录](../README.zh-CN.md) · [English](raw-memory.md) · **简体中文**

Block 是一次分配得到的字节存储。它不负责 C++ Object 的构造与析构。

## 1 · 自动释放

```cpp
#include <unimem/memory.h>

unimem::Memory& memory = unimem::Memory::global();
unimem::OwnedBlock block = memory.make_block(4096, 64);
block.resize(8192);
void* data = block.data();
```

`OwnedBlock` 保存大小与 Alignment，只能移动。销毁时自动释放；扩容失败保留旧 Block，新增加的字节不保证清零。

| 方法 | 用途 |
| --- | --- |
| `data()` | 获取指针 |
| `size()` | 请求的字节数 |
| `alignment()` | 分配时指定的 Alignment |
| `resize(bytes)` | 改变大小，保留旧内容的有效范围 |

## 2 · Alignment

| 写法 | 保证 |
| --- | --- |
| `allocate(100)` | 默认按 `alignof(std::max_align_t)` 对齐 |
| `allocate(100, 64)` | 起始地址为 64 的倍数 |
| `create<T>()` | 自动使用 `alignof(T)` |

Alignment 是地址对齐要求，必须为有效的 2 的幂。默认满足普通基本类型；需要更大 Alignment 的类型使用 Object API 或显式指定。

## 3 · 手动分配

```cpp
void* bytes = memory.allocate_zeroed(64, 32);
bytes = memory.reallocate_zeroed(bytes, 64, 128, 32);
memory.deallocate(bytes, 128, 32);
```

| 接口 | 作用 |
| --- | --- |
| `allocate(bytes, alignment)` | 分配，不初始化 |
| `allocate_zeroed(bytes, alignment)` | 分配，请求范围清零 |
| `reallocate(ptr, old, next, alignment)` | 改大小，保留旧内容的有效范围 |
| `reallocate_zeroed(ptr, old, next, alignment)` | 同上，新增请求范围清零 |
| `deallocate(ptr, bytes, alignment)` | 释放，传回原大小与 Alignment |

由**同一个 `Memory`** 释放。重分配后使用返回指针；失败时旧 Block 仍有效。零字节分配返回 `nullptr`，重分配到零会释放并返回 `nullptr`。Container 的零元素分配可能返回可配对释放的非空指针。

## 错误处理

| 情况 | 结果 |
| --- | --- |
| 分配失败 | `std::bad_alloc` |
| 非法 Alignment | `std::invalid_argument` |
| 类型化数量溢出 | `std::length_error` |
| 错误 Memory、大小、Alignment，或混用 Native 指针 | 违反约定，不保证检测 |

下一步：[Heap](heap.zh-CN.md) · [API](../api-reference.zh-CN.md)
