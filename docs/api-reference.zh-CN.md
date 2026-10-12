# 接口参考

[目录](README.zh-CN.md) · [English](api-reference.md) · **简体中文**

命名空间：`unimem`。包含 `<unimem/all.h>` 即可使用完整纯头文件分配接口，并链接 CMake `UniMemory::UniMemory` 接口目标。`Memory` 为非模板抽象基类，提供虚析构函数；`Global`、`Heap`、`Stack` 为 final 派生类。分配、所有权、Allocator/PMR 与统计接口可通过 `Memory&` 使用。

## 1 · 创建 Memory

| 接口 | 返回值与用途 |
| --- | --- |
| `Memory::global(statistics = Disabled)` | `Global&`，进程生命周期内唯一的单例，使用 CMake 所选后端 |
| `Heap(statistics = Disabled)` / `Memory::heap(statistics = Disabled)` | 独立 `Heap`，所有后端均支持 |
| `Stack(span<byte>, statistics = Disabled)` / `Memory::stack(span<byte>, statistics = Disabled)` | `Stack`，固定 Buffer，单线程借用 |
| `Stack(void*, size_t, statistics = Disabled)` / `Memory::stack(void*, size_t, statistics = Disabled)` | 同一策略的指针与大小形式 |

`Memory` 不可直接实例化。`Global` 构造函数私有，仅通过 `Memory::global()` 获取，进程生命周期内共享同一份统计和 PMR 资源。`Heap`、`Stack` 构造函数公开，须比其 Owner、Container、Allocator/PMR 适配器及共享/弱引用控制块存活更久。这些类不可复制或移动。Heap/Stack 的释放顺序见[生命周期](compatibility.zh-CN.md#生命周期)。

## 2 · 配置与查询

| 接口 | 含义 |
| --- | --- |
| `MemoryKind::{Global, Heap, Stack}` / `kind()` | 当前模式 |
| `Memory::config(release_delay_ms)` | 设置统一的闲置页回收延迟 |
| `statistics()` / `reset_statistics()` | 返回五字段值；清除五个字段，不释放存储 |

`config(release_delay_ms)` 接受毫秒数；-1 关闭自动页回收。统计由创建参数或实例的 `set_statistics()` 独立设置；OFF 构建拒绝 Basic。

源码配置必填 `UNIMEMORY_BACKEND=standard|mimalloc|jemalloc`、`UNIMEMORY_STATISTICS=ON|OFF`。ON 允许所有 Memory 模式运行时开启 Basic；OFF 拒绝 Basic 配置。所有模式默认 Disabled；Global 在首次获取时选择统计模式；Heap、Stack 在构造时选择各自模式。通过 `set_statistics()` 改变已有实例的统计模式。

`Statistics` 包含五个请求计数字段；Disabled 返回五个零值。
本库不自动替换普通 `new/delete` 或未适配的 Container。

## 3 · Bytes 与 Block

默认对齐 `alignof(std::max_align_t)`；大小使用 `size_t`。

| 接口 | 行为 |
| --- | --- |
| `allocate(bytes, alignment)` | 返回 `void*`，大小 0 返回空指针 |
| `allocate_zeroed(...)` | 请求字节清零 |
| `reallocate(ptr, old_bytes, new_bytes, alignment)` | 保留前缀，失败保留原块，大小 0 逻辑释放 |
| `reallocate_zeroed(...)` | 新增请求字节清零 |
| `deallocate(ptr, bytes, alignment)` | noexcept，使用原 Memory、大小和对齐配对 |
| `make_block(bytes, alignment)` | 可移动的 `OwnedBlock` |

OwnedBlock 提供 `data()`、`size()`、`alignment()`、`resize()`。扩容不默认清零。
Stack 单块释放不收回 Buffer 空间。

## 4 · Object、Array、Owner、Container

| 接口 | 用途 |
| --- | --- |
| `allocate_objects<T>(count = 1)` / `deallocate_objects(ptr, count = 1)` | 类型化原始存储，不构造 Object |
| `create<T>(args...)` / `destroy(ptr)` | 构造、销毁 Object；保持原具体类型 |
| `create_array<T>(count)` / `destroy_array(ptr, count)` | 值初始化 Array，释放需要原数量 |
| `make_unique<T>(args...)` | `Unique<T>` |
| `make_unique_array<T>(count)` | `UniqueArray<T>` |
| `make_shared<T>(args...)` | `std::shared_ptr<T>` |
| `make_shared<T[]>(count)` | `std::shared_ptr<T[]>`，共享数组 |
| `adopt_unique(ptr)` | 接管同一 Memory 的已有 Object，保持原具体类型 |
| `adopt_unique_array(ptr, count)` | 接管已有 Array，另需原数量 |
| `allocator<T>()` | 标准 `Allocator<T>` |
| `resource()` | `std::pmr::memory_resource*` |

对象析构不得抛异常。所有权与接管规则见[对象指南](guides/objects.zh-CN.md)，资源选择见[容器指南](guides/containers.zh-CN.md)。

## 5 · Heap 与 Stack 管理

| 接口 | 支持模式与用途 |
| --- | --- |
| `reset()` | Heap/Stack，整体释放或回退；计数重新开始 |
| `collect()` | Heap，尝试回收闲置资源，保留在用分配 |
| `owns(ptr)` | Heap，本 Heap 的有效分配起始地址；空指针 false |
| `mark()` | Stack，返回 `Stack::Mark` |
| `rewind(mark)` | Stack，回退，所有旧标记失效 |
| `used()` / `capacity()` | Stack，Buffer 占用与总容量 |

`owns()` 不验证任意指针。Stack 支持相同的请求计数，不提供原生指标。
Heap 管理接口仅属于 `Heap`，Stack 管理接口仅属于 `Stack`；`Memory` 与 `Global` 不提供这些接口。

## 6 · 错误

| 条件 | 行为 |
| --- | --- |
| 分配或容量不足 | `bad_alloc`；重分配保留原块 |
| 无效分配对齐 | 检查开启时抛 `invalid_argument`；OFF 时属于调用方前置条件 |
| 无效统计模式、OFF 构建的 Basic 配置、无效 Buffer 或 Mark | `invalid_argument` |
| 数量或标记计数溢出 | `length_error` |
| 原生控制失败 | `runtime_error` |
| 移动后 Block 扩容 | `logic_error` |
| 无效指针、释放不配对、提前 reset/销毁 | 违反调用契约，不保证检测 |

[构建](getting-started.zh-CN.md) · [生命周期](compatibility.zh-CN.md)
