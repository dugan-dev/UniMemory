# 对象与所有权

[目录](../README.zh-CN.md) · [English](objects.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#对象)。

## 构造与清理

| 情况 | 约定 |
| --- | --- |
| 对象构造 | 参数直接转发给构造函数 |
| 数组构造 | 每个元素通过 `T()` 值初始化 |
| `create_array()` 构造函数抛异常 | 逆序析构已完成的元素，再释放存储空间 |
| 析构函数 | 对象辅助接口要求析构函数不抛异常 |
| 手动销毁 | 使用同一个 Memory、原始具体类型和原数组数量 |
| 原始存储 | `allocate_objects<T>()` 不构造对象 |

保留分配起始指针。以 Derived 创建的对象不能交给 `destroy(Base*)`，即使 Base 有虚析构函数。普通 `new`/`delete` 不能与这些接口混用。原始 `reallocate()` 不调用对象的移动构造函数。

## 接管与多态

| 操作 | 规则 |
| --- | --- |
| `adopt_unique()` / `adopt_unique_array()` | 仅接管同一个 Memory 创建的对象，或匹配的智能指针通过 `release()` 交出的对象 |
| 接管 | 转移清理责任，不分配、不构造 |
| 空 Unique | 合法，持有对象前先赋予已绑定的所有者 |
| 非空但未绑定的 Deleter | 终止程序，因为没有可用于清理的 Memory |
| 数组 `reset(new_pointer)` | 保留旧元素数量，数量变化时重新接管并赋值 |
| 共享所有权的多态 | 将 `make_shared<Derived>()` 转成 `shared_ptr<Base>`，仍按具体类型清理 |
| 独占所有权的多态 | 保留 `Unique<Derived>`，`Base*` 仅作为非拥有的访问指针 |

## 生命周期

```mermaid
flowchart LR
    A[Memory] --> B[对象与智能指针]
    B --> C[销毁所有者与弱引用]
    C --> D[重置或销毁 Heap / Stack]
```

所有者保留 Memory 引用。必须先销毁它们，再销毁 Heap/Stack，或通过 reset、rewind 使存储失效。残留的 `weak_ptr` 也会保留共享指针控制块。

Stack 中构造失败会析构已完成的对象，但不回退缓冲区用量。所有受影响的所有者销毁后才能 rewind。

[容器约定](containers.zh-CN.md) · [生命周期与线程](../compatibility.zh-CN.md) · [API](../api-reference.zh-CN.md)
