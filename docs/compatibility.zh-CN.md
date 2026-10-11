# 生命周期与兼容性

[目录](README.zh-CN.md) · [English](compatibility.md) · **简体中文**

## 生命周期

| 类型 | 规则 |
| --- | --- |
| Global | 库管理，静态析构期间仍有效；不可删除或显式析构 |
| Heap | Owner、Container、weak_ptr 控制块全部释放后，再 reset 或析构 |
| Stack | Buffer 比 Memory 活得更久；先销毁受影响的 Owner、Container，再回退 |

```mermaid
flowchart LR
    A[Create Memory] --> B[Create Owners / Containers]
    B --> C[Destroy Owners / Containers]
    C --> D[Reset / Rewind / Destroy Memory]
```

Global、Heap 支持并发分配释放。Object 读写需自行同步。reset、collect、析构要求独占访问；Stack 单线程。

| 访问 | 同步要求 |
| --- | --- |
| Global / Heap 上不同分配 | 分配、释放可并发 |
| Basic 统计快照 | 原子计数；不同字段可能来自不同时刻 |
| Backend 诊断统计 | 暂停 Memory/Process 范围内的活动和线程清理，包含原生调用 |
| 同一 Object、OwnedBlock、Container | 调用方协调读写 |
| 独立的 shared_ptr / weak_ptr 句柄 | 标准控制块同步；Object 数据另需保护 |
| 修改同一个 shared_ptr 句柄 | 调用方同步，或使用标准 atomic shared_ptr |
| 向其他线程交接所有权 | Mutex、barrier 或 release/acquire 发布；不使用无同步指针交接 |
| Heap collect/reset/析构 | 无并发 Memory 操作，不访问已回收存储 |

## 分配与释放配对

使用原 Memory、当前请求大小及原对齐参数释放。不可混用 delete、free 或另一个 Heap。Smart Pointer、OwnedBlock 记住来源；原始指针不会。

原始 Object 还须保留原具体类型和分配起始指针。接管接口只绑定 Memory，不检查任意指针的来源。

reset、rewind 只回收存储，不调用 Object 析构。旧指针、仍存活的 Container 和 weak_ptr 控制块不可继续使用已回收存储。

## 动态库

同一程序使用同一配置时，纯头文件 inline Global 状态在各翻译单元共享。不要假定独立加载的 DLL/插件一定共用实例，身份取决于符号可见性与平台链接器。跨边界传递原 Memory/资源，保持其定义模块加载，并用原实例配对释放。
跨动态库的 C++ Object、Container 还要求兼容的编译器、标准库及 Runtime ABI。

使用 Memory、智能指针、Allocator 或 PMR 资源期间，定义模块和动态链接的 SDK 必须保持加载；UniMemory 不提供单独的共享库 ABI。

## 标准库行为

`make_shared<T>()` 使用 `std::allocate_shared`，Object 和控制块遵循所用标准库。
实测 libstdc++ 13.3：`make_shared<T[]>(count)` 的元素构造抛异常时，已构造元素按正序析构。
原生 `std::make_shared` 同样如此，与 C++20 的逆序要求不符（[LWG 3005](https://cplusplus.github.io/LWG/issue3005)）。
已构造元素仍全部析构，存储按对应 Memory 类型释放。

需要确定的逆序回滚时，使用 `create_array` 或 `make_unique_array`。尚未核实此标准库问题的修复版本。

## 版本

**0.0.1** 使用 C++20。0.x 不承诺稳定 ABI；头文件、生成配置与 SDK 库必须匹配，配置或布局变化时重新编译使用者。

目前不承诺长期支持或修复回移时间表。应固定源码修订，并在升级时验证编译器、标准库、运行库和后端配置。不同修订即使都标为 0.0.1，通过 CMake 版本检查也不等于 ABI 兼容。[源码快照与安装](getting-started.zh-CN.md#获取源码)

## 后端扩展

`Backend` 是 `Standard`、`Mimalloc`、`Jemalloc` 的封闭集合，一次 CMake 配置选择一种集成；不提供运行时注册或分配器插件 ABI。新增后端需要经过审查的源码修改、明确的依赖决策、能力映射以及接口和包测试。`std::pmr::memory_resource` 让消费者通过已有接口选择存储，不会注册新的 UniMemory 后端。
