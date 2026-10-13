# 容器与 PMR

[首页](../README.zh-CN.md) · [English](containers.md)

使用 C++20，包含 `<unimem/all.h>`，链接 `UniMemory::UniMemory`。Global、Heap、Stack 提供 `allocator<T>()` 给标准分配器感知容器，`resource()` 给 PMR。不支持注入分配器的库仍使用自身分配器。

## 标准容器

```cpp
#include <unimem/all.h>
#include <vector>

int main() {
    unimem::Global memory;
    std::vector<int, unimem::Allocator<int>> values(memory.allocator<int>());
    values.push_back(42);
    auto copy = values; // 保留此 Memory 的分配器
    return copy.front() == 42 ? 0 : 1;
} // 容器先于 memory 销毁
```

适配器只有绑定同一 Memory 才比较相等。不同 Global 共享原生分配设施，但分配器及资源身份独立。普通容器复制保留 UniMemory 分配器。分配器不等时，移动赋值可能重新分配并移动元素，而非接管存储。标准容器要求时，swap 的分配器或资源必须相等。

## PMR 与嵌套分配

```cpp
#include <unimem/all.h>
#include <memory_resource>
#include <string>
#include <vector>

int main() {
    unimem::Global memory;
    std::pmr::vector<std::pmr::string> names(memory.resource());
    names.emplace_back("Ada"); // 字符串接收 vector 的资源
    std::pmr::vector<std::pmr::string> copy(names, memory.resource());
    return copy.front() == "Ada" ? 0 : 1;
}
```

`vector<std::string, Allocator<...>>` 只适配 vector 存储；普通字符串仍使用自身分配器。PMR 的分配器感知构造能把资源传给 `pmr::string`。PMR 复制构造通常选择默认 PMR 资源；显式传入目标资源以保留期望的 Memory。

## 组合临时资源

```cpp
#include <unimem/all.h>
#include <memory_resource>
#include <vector>

int main() {
    unimem::Global memory;
    std::pmr::monotonic_buffer_resource pool(memory.resource());
    std::pmr::vector<int> temporary(&pool);
    temporary.push_back(42);
    std::pmr::vector<int> copy(temporary, &pool);
    return copy.front() == 42 ? 0 : 1;
} // 依次销毁容器、pool、memory
```

分配路径为容器 → 标准 PMR 资源 → Memory 资源 → 后端。中间资源有自己的回收和线程规则；线程安全的后端不会让 `monotonic_buffer_resource` 自动线程安全。

## 生命周期与兼容性

Memory 不可复制或移动。保持其地址稳定，直到容器、适配器、所有者及 shared/weak 控制块全部消失。先销毁容器，再销毁中间 PMR 资源，最后销毁 Memory。Heap reset/销毁或 Stack rewind/reset 前先销毁受影响消费者；这些操作回收存储但不执行元素析构。

Stack 借用可写、固定容量且比自身存活更久的缓冲区。逐次释放保留空间；只用单线程，不创建活动的重叠缓冲区分配器。Global/Heap 支持并发分配/释放；同一容器访问由调用方同步。Heap collect/reset/销毁需要独占访问。

跨 DLL/插件边界保留原 Memory/资源身份、生命周期和分配配对。消费者存在期间保持其定义模块和动态 SDK 加载。传递 C++ 容器要求编译器、标准库和运行时 ABI 兼容。UniMemory 0.x 不保证稳定 ABI：使用匹配的头文件、生成配置和库，配置或布局改变后重新构建消费者。
