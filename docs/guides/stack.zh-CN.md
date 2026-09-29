# Stack 分配

[索引](../README.zh-CN.md) · [English](stack.md) · **简体中文**

## 借用固定 Buffer

```cpp
#include <unimem/memory.h>

alignas(std::max_align_t) std::byte buffer[4096];
unimem::Memory scratch = unimem::Memory::stack(buffer);
unimem::Memory::Mark checkpoint = scratch.mark();
{
    unimem::OwnedBlock block = scratch.make_block(128, 64);
}
scratch.rewind(checkpoint);
```

Stack 表示标记回退的分配策略，工厂不在内部创建 OS 栈数组。
Buffer 可写、固定容量、单线程使用，由调用方管理，必须比 Memory 活得更久。
Object、Owner、Container、PMR 都使用同一套 Memory 方法。

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

回退内层 Mark 后，外层 Mark 也失效。本接口采用单代标记，不支持保存多层 Mark 后逐层回退。

## 重分配与失败

| 场景 | 结果 |
| --- | --- |
| 容量不足 | `bad_alloc`，位置不变，不回退到 Heap |
| 块结束于当前位置且空间足够 | 原位扩展 |
| 其他扩容 | 同一 Buffer 内另分配并复制 |
| 缩容或单块释放 | 保留已占用空间 |
| 重分配失败 | 原指针、数据及位置保持有效 |

Stack 没有 Backend 或原生统计。`used()` 表示 Buffer 占用，包含填充和单块释放后保留的空间。

下一步：[Container](containers.zh-CN.md) · [接口](../api-reference.zh-CN.md)
