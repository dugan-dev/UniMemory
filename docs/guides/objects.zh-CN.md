# Object / Array

[目录](../README.zh-CN.md) · [English](objects.md) · **简体中文**

创建 C++ Object，自动调用构造与析构函数。日常使用优先选择 Smart Pointer。

## 1 · 自动管理

```cpp
#include <unimem/memory.h>

struct Point {
    float x;
    float y;
};

unimem::Memory& memory = unimem::Memory::global();
unimem::Unique<Point> point = memory.make_unique<Point>(1.0f, 2.0f);
unimem::UniqueArray<Point> points = memory.make_unique_array<Point>(8);
std::shared_ptr<Point> shared = memory.make_shared<Point>(1.0f, 2.0f);
```

| 接口 | 返回类型 | 何时销毁 |
| --- | --- | --- |
| `make_unique<T>(args...)` | `Unique<T>` | 唯一 Owner 销毁时 |
| `make_unique_array<T>(count)` | `UniqueArray<T>` | 唯一 Owner 销毁时，自动记住数量 |
| `make_shared<T>(args...)` | `std::shared_ptr<T>` | 最后一个 Shared Owner 销毁时 |

单个 Object 无须传数量 `1`；Array 必须指定数量，元素按 `T()` 初始化。

## 2 · 手动管理

```cpp
Point* point = memory.create<Point>(1.0f, 2.0f);
memory.destroy(point);

Point* points = memory.create_array<Point>(8);
memory.destroy_array(points, 8);
```

由**创建它的同一个 `Memory`** 销毁，保持创建时的具体类型、分配起始指针，Array 传回原数量。
`create<Derived>()` 的结果不能改用 `Base*` 调用 `destroy()`，即使 Base 有虚析构。
构造失败会清理已构造的元素；Stack 仍保留 Buffer 占用，需要在清理其他受影响 Owner 后回退或 reset。析构函数必须不抛异常。

## 接管已有 Object

```cpp
Point* raw = memory.create<Point>(1.0f, 2.0f);
unimem::Unique<Point> owner = memory.adopt_unique(raw);
unimem::UniqueArray<Point> array = memory.adopt_unique_array(memory.create_array<Point>(8), 8);
```

接管不分配、不构造 Object；仅用于同一 Memory 的 `create` 或匹配 Owner 的 `release` 结果，不接管普通 `new`。
默认 Unique 是空 Owner；赋值一个接管 Owner 后，Deleter 就绑定了 Memory。
非空 Owner 使用未绑定的 Deleter 会确定性终止。Array 的 `reset(new_pointer)` 沿用旧数量；数量变化时赋值新的接管 Owner。
多态共享所有权可用 `std::shared_ptr<Base> owner = memory.make_shared<Derived>();`；Unique 保留 `Unique<Derived>`，Base 指针仅作观察。

## 3 · 只分配存储

| 接口 | 分配内存 | 构造 Object |
| --- | :---: | :---: |
| `allocate_objects<T>(count = 1)` | ✓ | — |
| `create<T>(args...)` | ✓ | ✓ |

`allocate_objects` 配对 `deallocate_objects(ptr, count = 1)`。仅在自行管理 placement new 和析构时使用。

## 使用规则

- `Memory` 必须比 Object、Smart Pointer 和相关 `weak_ptr` 活得更久。
- `make_unique` 的指针不能交给普通 `delete`。
- `reallocate` 只移动字节，不能用来移动需要构造与析构的 Object。

下一步：[Container](containers.zh-CN.md) → [Block](raw-memory.zh-CN.md)
