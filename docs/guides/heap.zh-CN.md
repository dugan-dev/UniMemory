# 独立 Heap

[索引](../README.zh-CN.md) · [English](heap.md) · **简体中文**

## 创建与使用

```cpp
#include <unimem/memory.h>

struct Point { float x; float y; };
unimem::Memory heap = unimem::Memory::heap(unimem::Backend::Mimalloc);
{
    unimem::Unique<Point> point = heap.make_unique<Point>(1.0f, 2.0f);
    heap.collect();
}
heap.reset();
```

构建时启用 Backend；`capabilities(backend).heap` 查询支持情况。Standard 不支持独立 Heap。
独立 Heap 可以直接使用所有通用 Memory 方法。

## 成员操作

| 成员 | 用途 |
| --- | --- |
| `reset()` | 整体释放，Basic 统计重新开始 |
| `collect()` | 尝试回收原生闲置资源，不影响在用分配 |
| `owns(ptr)` | 查询一个在用分配是否属于当前 Heap |
| `statistics()` | 当前 Heap 的可选统一计数 |
| `backend_statistics()` | 可选原生指标；当前为开启 `config.stats` 的 jemalloc |

Memory 析构释放剩余 Heap 内存。析构与 reset 都不调用其中 Object 的析构函数。
先销毁 Owner、Container 和 weak_ptr 控制块。reset 失败保留原分配。
collect、reset、Memory 析构要求独占访问；并发分配释放受支持，Object 读写和所有权交接另需同步。
`owns(nullptr)` 为 false；其他输入必须是同一 Backend 的有效分配起始地址，不用于验证任意或悬空指针。
collect 不保证程序内存占用立即下降，也不承诺排空每个线程的缓存。

```mermaid
flowchart LR
    A[创建 Heap] --> B[使用 Object / Container / Block]
    B --> C[销毁 C++ Owner]
    C --> D[Reset 或销毁 Heap]
```

原生映射为 mimalloc Heap、关闭 tcache 的 jemalloc arena。Heap 可增长，但不保证各分配连续。

下一步：[Stack](stack.zh-CN.md) · [统计](statistics.zh-CN.md)
