# 接口参考

[索引](README.zh-CN.md) · [English](api-reference.md) · **简体中文**

命名空间：`unimem`。包含 `<unimem/memory.h>` 即可使用分配接口。

## 1 · 创建 Memory

| 接口 | 返回值与用途 |
| --- | --- |
| `Memory::global(Backend = Standard)` | `Memory&`，库管理的共享实例 |
| `Memory::configure_global(Backend, StatisticsMode)` | 首次获取前配置统计；之后改模式报错 |
| `Memory::heap(Backend, StatisticsMode = Disabled)` | `Memory`，独立 Heap，Standard 不支持 |
| `Memory::stack(span<byte>)` | `Memory`，固定 Buffer，单线程借用 |
| `Memory::stack(void*, size_t)` | 同一策略的指针与大小形式 |

构造函数隐藏；Memory 无虚函数，禁止复制和移动；工厂直接构造结果。
Heap 销毁/reset、Stack rewind/reset 前先销毁受影响的 Owner；整体释放不调用 Object 析构。

## 2 · 配置与能力查询

| 接口 | 含义 |
| --- | --- |
| `Backend::{Standard, Mimalloc, Jemalloc}` | 选择 Backend |
| `MemoryKind::{Global, Heap, Stack}` / `kind()` | 当前模式 |
| `backend()` | `optional<Backend>`，Stack 为空 |
| `available(backend)` | 构建是否启用该 Backend |
| `capabilities(backend)` | `available`、`heap`、`detailed_statistics`、`release_delay` |
| `memory.capabilities()` | `basic_statistics`、`detailed_statistics`、`reset`、`collect`、`owns`、`checkpoints`、`thread_safe`、`individual_reclaim` |
| `supports()` / `set_runtime_option()` | 查询或设置 Backend 全局选项 |
| `statistics()` / `backend_statistics()` | 可选的[统计数据](guides/statistics.zh-CN.md) |

运行选项为 `UnusedPageReleaseDelayMs`，单位毫秒。支持计数与开启计数是两回事。
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
| `make_unique<T>()` | `Unique<T>` |
| `make_unique_array<T>()` | `UniqueArray<T>` |
| `make_shared<T>()` | 标准 `shared_ptr<T>` |
| `adopt_unique(ptr)` | 接管同一 Memory 的已有 Object，保持原具体类型 |
| `adopt_unique_array(ptr, count)` | 接管已有 Array，另需原数量 |
| `allocator<T>()` | 标准 `Allocator<T>` |
| `resource()` | `std::pmr::memory_resource*` |

Object 析构必须 noexcept，构造失败会析构已构造元素；Stack 仍保留 Buffer 占用。
默认 Unique 是空 Owner；非空 Owner 使用未绑定的 Deleter，或非空 Array 的数量为零时，会确定性终止。
适配器按 Memory 地址判断相等。
Owner、Allocator、PMR 以及 weak_ptr 控制块不能超过 Memory 生命周期。

Basic 快照可用于并发监控；Backend 诊断快照要求暂停相应 Memory/Process 范围内的活动和线程清理。

## 5 · Heap 与 Stack 管理

| 接口 | 支持模式与用途 |
| --- | --- |
| `reset()` | Heap/Stack，整体释放或回退；Heap 计数重新开始 |
| `collect()` | Heap，尝试回收闲置资源，保留在用分配 |
| `owns(ptr)` | Heap，同 Backend 的有效分配起始地址；空指针 false |
| `mark()` | Stack，返回 `Memory::Mark` |
| `rewind(mark)` | Stack，回退，所有旧标记失效 |
| `used()` / `capacity()` | Stack，Buffer 占用与总容量 |

`owns()` 不验证任意指针。Stack 没有 Backend、Basic 计数或原生指标。
不支持的模式操作抛 `logic_error`。

## 6 · 错误

| 条件 | 行为 |
| --- | --- |
| 分配或容量不足 | `bad_alloc`；重分配保留原块 |
| 无效对齐、枚举、Buffer、Mark | 已验证处抛 `invalid_argument` |
| 类型化大小或 Mark 代数溢出 | `length_error` |
| Backend 未启用、Heap 不支持、原生控制失败 | `runtime_error` |
| 模式操作不支持、Global 已初始化后改模式、移动后 Block 扩容 | `logic_error` |
| 无效指针、释放不配对、提前 reset/销毁 | 违反调用契约，不保证检测 |

[构建](getting-started.zh-CN.md) · [生命周期](compatibility.zh-CN.md)
