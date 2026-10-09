# 容器与 PMR

[目录](../README.zh-CN.md) · [English](containers.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#快速开始)。

## 适配方式

| 方法 | 使用对象 |
| --- | --- |
| `allocator<T>()` | 接受标准分配器的容器 |
| `resource()` | PMR 容器和内存资源 |

未传入分配器的容器和第三方库仍使用原分配器。适配器只有绑定同一 Memory 才相等。

## 复制、移动与嵌套类型

| 情况 | 规则 |
| --- | --- |
| 普通容器复制 | 分配器适配器保留原 Memory |
| PMR 复制构造 | 通常使用默认 PMR 资源，显式传入目标资源才能保留选定的 Memory |
| 交换 | 标准容器要求时，两边的分配器或资源必须相等 |
| 不同分配器之间的移动赋值 | 可能重新分配并逐个移动元素，而非直接接管存储 |
| `vector<std::string, Allocator<...>>` | 仅适配 vector 存储，普通 string 仍使用自己的分配器 |
| `pmr::vector<pmr::string>` | 标准的分配器感知构造可将资源传播给内部 string |

## 组合临时内存资源

```cpp
#include <unimem/memory.h>

int main() {
    unimem::Memory& memory = unimem::Memory::global(unimem::Backend::Standard);

    // 在 UniMemory 之上组合标准 PMR 资源
    std::pmr::monotonic_buffer_resource pool(memory.resource());
    std::pmr::vector<int> temporary(&pool);
    temporary.push_back(42);

    // 复制时显式选择目标资源
    std::pmr::vector<int> copy(temporary, &pool);
    return copy.front() == 42 ? 0 : 1;
}
```

```mermaid
flowchart LR
    A[容器] --> B[标准 PMR 资源]
    B --> C[Memory 资源]
    C --> D[后端]
```

先销毁容器，再销毁 PMR 资源，最后销毁 Heap/Stack Memory。资源有自己的回收和线程约定。跨动态库传递 C++ 容器还要求编译器、标准库和运行时 ABI 兼容。

[生命周期与线程](../compatibility.zh-CN.md) · [Stack 约定](stack.zh-CN.md) · [API](../api-reference.zh-CN.md)
