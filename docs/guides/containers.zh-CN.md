# Container

[目录](../README.zh-CN.md) · [English](containers.md) · **简体中文**

让标准 Container 从指定 `Memory` 获取内存。两条路径按项目接口选择。

## 1 · PMR

```cpp
#include <unimem/memory.h>
#include <vector>

unimem::Memory& memory = unimem::Memory::global();
std::pmr::vector<int> values(memory.resource());
values.push_back(42);
```

PMR 是标准库的运行时 Allocator 接口。`resource()` 返回 `std::pmr::memory_resource*`；标准库已有 `std::pmr::vector`、`std::pmr::string` 等类型。

## 2 · 普通 std Container

```cpp
std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
values.push_back(42);
```

| 路径 | 接口 | 特点 |
| --- | --- | --- |
| PMR | `resource()` | 可在运行时传递 Resource |
| 普通 std | `allocator<T>()` | Allocator 是 Container 类型的一部分，无 PMR 虚调用 |

只创建 `Memory` 不会改变已有 Container；必须显式传入 Allocator 或 Resource。

## 3 · 临时 Container

```cpp
std::pmr::monotonic_buffer_resource pool(memory.resource());
std::pmr::vector<int> temporary(&pool);
```

```mermaid
flowchart LR
    C[Container] --> P[PMR Resource]
    P --> M[Memory]
    M --> B[Backend]
```

先销毁 `temporary`，再销毁 `pool`；引用的 Memory 保持有效。PMR Resource 自行决定空间复用、回收和线程规则。

## 使用规则

| 场景 | 注意 |
| --- | --- |
| PMR 复制构造 | 通常使用默认 Resource；需指定目标 Resource 时显式传入 |
| 普通 Container 复制 | 保留适配器引用的 Memory |
| 不同 Allocator 的移动赋值 | 可能重新分配并移动元素 |
| Container 交换 | 必须满足标准对 Allocator 相等性的要求 |
| `vector<std::string>` | vector 使用指定 Allocator；string 仍使用自己的分配路径 |
| `pmr::vector<pmr::string>` | Resource 可通过标准 Allocator 规则传播到 string |

`Memory` 必须比所有相关 Container 活得更久。跨动态库使用还需保持编译器、标准库和运行库 ABI 一致。

下一步：[Block](raw-memory.zh-CN.md) → [Backend](backends.zh-CN.md)
