# 内存类型：Global、Heap 与 Stack

[首页](../README.zh-CN.md) · [English](heap.md)

使用 C++20，包含 `<unimem/all.h>` 并链接 `UniMemory::UniMemory`。直接构造 Global、Heap 或 Stack；通过 `Memory&` 使用通用的分配、对象、容器和统计接口。实例不可复制或移动。

| 类型 | 构造 | 存储与控制 |
| --- | --- | --- |
| `Global` | `Global(statistics = Disabled)` | 共享原生分配器，调用方拥有上下文 |
| `Heap` | `Heap(statistics = Disabled)` | 独立分配组；`reset`、`collect`、`owns` |
| `Stack` | `Stack(span<byte>, statistics = Disabled)` 或 `Stack(void*, size_t, statistics = Disabled)` | 借用固定缓冲区；`mark`、`rewind`、`reset`、`used`、`capacity` |

各实例的请求统计和 PMR 资源身份独立。统计默认 Disabled；Basic 要求 `UNIMEMORY_STATISTICS=ON`。调用 `set_statistics()` 或 `reset_statistics()` 前暂停 Memory 操作。

## Global

Global 构造函数公开。不同实例共享 CMake 选择的原生分配器，但计数及适配器身份独立。构造不替换普通 new/delete 或未适配容器。销毁不批量释放尚存原生分配；须在原 Global 存活时通过它释放。

```cpp
#include <unimem/all.h>
#include <cassert>

int main() {
    unimem::Global first;
    unimem::Global second;
    assert(first.resource() != second.resource());
    unimem::Memory& memory = first;
    auto value = memory.make_unique<int>(42);
    return *value == 42 ? 0 : 1;
}
```

跨翻译单元传递调用方拥有的 Global 引用。跨 DLL/插件保留原 Memory/资源身份，用户全部消失前保持定义模块和 SDK 加载。跨库 C++ 对象要求编译器、标准库和运行时 ABI 兼容。

## Heap

所有后端支持独立 Heap：原生 mimalloc heap、独立 jemalloc arena 或 Standard 分配组。不保证物理页隔离。

| 操作 | 行为 |
| --- | --- |
| `reset()` | 释放所有块并清空请求计数；失败保留分配 |
| `collect()` | 尝试回收空闲内存，保留活跃分配 |
| `owns(ptr)` | 判断活跃分配起始指针归属；空指针为 false |

`owns()` 不是指针校验器：其他指针必须是同后端活跃分配起始地址，不能是任意或悬空地址。collect 未必立即降低内存占用或清空全部线程缓存。

```cpp
#include <unimem/all.h>
#include <cassert>

int main() {
    unimem::Heap heap;
    {
        auto block = heap.make_block(64);
        assert(heap.owns(block.data()));
        heap.collect(); // 活跃块仍有效
    }
    heap.reset(); // 消费者已销毁
}
```

reset/销毁前先销毁对象、容器、所有者及剩余 weak 控制块。批量释放不执行 C++ 析构。

## Stack

Stack 使用**借用的可写固定容量缓冲区**，不是线程调用栈。缓冲区须比 Stack 及其用户存活更久。仅单线程，不创建活动的重叠缓冲区分配器。容量耗尽抛 `std::bad_alloc` 且偏移不变，不回退到 Heap。无效缓冲区配置抛 `std::invalid_argument`。

| 操作 | 行为 |
| --- | --- |
| `allocate()` | 推进偏移，包含对齐填充 |
| `deallocate()` / 所有者销毁 | 保留缓冲区空间；所有者销毁仍执行对象析构 |
| `mark()` | 保存偏移，返回不透明 `Stack::Mark` |
| `rewind(mark)` | 回到保存偏移，全部旧标记失效 |
| `reset()` | 回到零、清空计数、全部旧标记失效 |
| `used()` / `capacity()` | 已消耗缓冲区字节 / 总字节 |

rewind/reset 前先销毁受影响对象及用户；操作自身不执行析构。外来或过期标记抛 `std::invalid_argument`；标记不能比 Stack 存活更久。回退内层标记也使外层标记失效，因此不能逐层使用旧标记回退。

```cpp
#include <unimem/all.h>
#include <array>
#include <cassert>
#include <cstddef>
#include <span>

int main() {
    alignas(std::max_align_t) std::array<std::byte, 1024> buffer{};
    unimem::Stack scratch{std::span<std::byte>{buffer}};
    auto checkpoint = scratch.mark();
    {
        auto value = scratch.make_unique<int>(42);
        assert(*value == 42);
    } // 析构对象，保留缓冲区空间
    assert(scratch.used() > 0);
    scratch.rewind(checkpoint);
    assert(scratch.used() == 0);
}
```

块结束于当前偏移且容量足够时原地增长；其他增长在同一缓冲区重新分配并复制。缩小/释放保留空间。调整失败保留原指针、内容和偏移。对象构造失败也保留消耗空间。used 包含填充和保留存储，与请求计数衡量不同量。Stack 无原生统计。

## 线程与配置

Global、Heap 允许并发分配/释放。对象、容器、块访问和所有权发布另行同步。Heap collect/reset 及所有 Memory 销毁需要独占访问，不允许并发操作或访问已回收存储。Stack 仅单线程。Memory 必须比所有者、容器、适配器及 shared/weak 控制块存活更久。

CMake 每配置选择一次 standard、mimalloc 或 jemalloc；没有运行时注册或分配器插件 ABI。`Memory::config(release_delay_ms)` 设置后端级空闲页释放策略；-1 禁用自动回收，启动工作线程前配置。新后端要求修改源码并明确依赖决策。翻译单元使用匹配的头文件、生成配置和库；UniMemory 0.x 不保证稳定 ABI。
