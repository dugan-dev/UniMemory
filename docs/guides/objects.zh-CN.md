# 分配与所有权

[首页](../README.zh-CN.md) · [English](objects.md)

使用 C++20，包含 `<unimem/all.h>`，链接 `UniMemory::UniMemory`。直接构造 `Global`、`Heap` 或 `Stack`。对象、所有者、容器、适配器及 shared/weak 控制块必须先结束使用，再销毁 Memory 或重置存储。

## 字节与拥有型块

| API | 行为 |
| --- | --- |
| `allocate(bytes, alignment)` | 未初始化字节 |
| `allocate_zeroed(bytes, alignment)` | 请求字节清零 |
| `reallocate(ptr, old_bytes, new_bytes, alignment)` | 保留 `min(old_bytes, new_bytes)` 字节；指针可能变化 |
| `reallocate_zeroed(...)` | 额外清零新增请求字节 |
| `deallocate(ptr, bytes, alignment)` | 通过原 Memory 释放 |
| `make_block(bytes, alignment)` | 只能移动的 `OwnedBlock`，自动释放 |

默认对齐为 `alignof(std::max_align_t)`，必须为非零的二次幂。过度对齐类型使用 `alignof(T)`；字节数不必是对齐倍数。释放时使用当前请求大小和原对齐值。不得混用 `delete`、`free` 或另一个 Memory。

`OwnedBlock` 提供 `data()`、`size()`、`alignment()`、`resize(new_bytes)`。增长部分未初始化。成功调整大小或替换所有权后，丢弃缓存的数据指针。字节释放不执行对象析构；原始调整大小不执行对象移动构造。

```cpp
#include <unimem/all.h>
#include <cassert>
#include <cstring>

int main() {
    unimem::Global memory;
    auto block = memory.make_block(16);
    std::memset(block.data(), 7, block.size());
    block.resize(32);
    assert(static_cast<unsigned char*>(block.data())[0] == 7);
    void* raw = memory.allocate_zeroed(16, 64);
    try {
        raw = memory.reallocate_zeroed(raw, 16, 32, 64);
    } catch (...) {
        memory.deallocate(raw, 16, 64);
        throw;
    }
    memory.deallocate(raw, 32, 64);
} // block 先于 memory 销毁
```

## 对象与类型化存储

| API | 约定 |
| --- | --- |
| `create<T>(args...)` / `destroy(ptr)` | 转发构造参数；使用原具体类型析构 |
| `create_array<T>(count)` / `destroy_array(ptr, count)` | 以 `T()` 值初始化；使用原元素数 |
| `allocate_objects<T>(count = 1)` / `deallocate_objects(ptr, count = 1)` | 类型化数组存储，不构造或初始化元素 |

对象辅助函数要求析构不抛异常。保留分配起始指针：即使基类有虚析构，也不能把 Derived 对象交给 `destroy(Base*)`。构造失败会释放存储；数组构造失败按逆序析构已完成元素。在 Stack 上释放仍保留已消耗缓冲区空间。

类型化分配及 `Allocator<T>::allocate()` 按 C++20 建立数组存储，包括 Stack 重用后。访问非平凡元素前先构造，释放存储前先析构。

```cpp
#include <unimem/all.h>
#include <memory>
#include <string>

int main() {
    unimem::Global memory;
    int* values = memory.create_array<int>(3); // 三个零
    memory.destroy_array(values, 3);
    auto* text = memory.allocate_objects<std::string>();
    try {
        std::construct_at(text, "hello");
    } catch (...) {
        memory.deallocate_objects(text);
        throw;
    }
    std::destroy_at(text);
    memory.deallocate_objects(text);
}
```

## 智能所有权与接管

| API | 返回 |
| --- | --- |
| `make_unique<T>(args...)` | `Unique<T>` |
| `make_unique_array<T>(count)` | `UniqueArray<T>`，元素值初始化 |
| `make_shared<T>(args...)` / `make_shared<T[]>(count)` | 标准共享指针 / 共享数组 |
| `adopt_unique(ptr)` / `adopt_unique_array(ptr, count)` | 转移清理责任，不分配或构造 |

只接管同一 Memory 创建或匹配所有者释放的对象，保留原具体类型和数组长度。接管不检查任意指针来源。默认空 Unique 有效；赋予对象前先赋予绑定的所有者。非空且未绑定的删除器违反约定，checks ON 时终止。数组 `reset(new_pointer)` 保留旧长度；长度变化时赋予新接管的数组。

```cpp
#include <unimem/all.h>
#include <memory>

struct Base { virtual ~Base() = default; };
struct Derived : Base { int value = 42; };

int main() {
    unimem::Global memory;
    auto unique = memory.make_unique<Derived>();
    Base* view = unique.get(); // 所有者保留为 Unique<Derived>
    (void)view;
    std::shared_ptr<Base> shared = memory.make_shared<Derived>();
    auto adopted = memory.adopt_unique(memory.create<int>(42));
    auto array = memory.make_unique_array<int>(3);
} // 所有者和控制块先于 memory 销毁
```

共享指针转换为 Base 仍保留具体类型清理。剩余 weak 指针保留控制块，必须先于 Memory 销毁或重置消失。`make_shared` 委托给 `std::allocate_shared`，回滚遵循标准库。已测试 libstdc++ 13.3 对部分构造共享数组按正序析构；要求逆序回滚时使用 `create_array` 或 `make_unique_array`。

## 错误与线程

零字节分配返回空指针；释放空指针无操作。重新分配空指针等同分配；调整至零逻辑释放并返回空指针。调整失败保留原指针和内容。分配或容量不足抛 `std::bad_alloc`，类型化数量溢出抛 `std::length_error`，调整已移动块抛 `std::logic_error`。无效对齐在 checks ON 时抛 `std::invalid_argument`，OFF 时为调用方前置条件。无效指针和不匹配释放未必被检测。

Global、Heap 支持并发分配/释放；Stack 仅单线程。同一对象或块的访问需另行同步。不同 shared/weak 句柄遵循标准控制块同步；修改同一句柄需同步或标准原子共享指针接口。跨线程发布所有权使用互斥锁、屏障或 release/acquire 同步。Heap reset 或 Stack rewind/reset 前先销毁受影响对象、所有者、容器和 weak 引用，这些操作不调用析构。
