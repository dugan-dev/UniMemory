# 统计

[索引](../README.zh-CN.md) · [English](statistics.md) · **简体中文**

## 选择查询

| 查询 | 范围 | 内容 |
| --- | --- | --- |
| `statistics()` | 当前 Memory | 成功次数、请求字节、峰值 |
| `backend_statistics()` | Backend 或独立 Heap | 后端可用的原生指标 |
| Stack 的 `used()` | 当前 Buffer | 占用字节，包含填充和保留空间 |

## 开启统一计数

```cpp
unimem::Memory::configure_global(unimem::Backend::Mimalloc,
    unimem::StatisticsMode::Basic);
unimem::Memory& memory = unimem::Memory::global(unimem::Backend::Mimalloc);
unimem::Memory heap = unimem::Memory::heap(unimem::Backend::Mimalloc,
    unimem::StatisticsMode::Basic);
```

Global 首次获取前配置；初始化后切换统计模式抛 `logic_error`，同值重复配置允许。默认 Disabled。

Global 汇总所有通过该实例的请求；Heap 单独计数，reset 后重新开始。原生分配不会自动进入这组计数。关闭统计没有统计原子更新；Basic 每次成功分配、释放和重分配更新原子计数。并发快照的不同字段可能来自略有不同的时刻。

| 字段 | 含义 |
| --- | --- |
| `allocations` / `deallocations` / `reallocations` | 成功操作次数 |
| `live_bytes` | 尚未逻辑释放的请求字节 |
| `peak_live_bytes` | 请求字节峰值 |

## Backend 详细统计

先查询实例的 `memory.capabilities().detailed_statistics`。Stack 两种统计均返回空值；`statistics()` 仅在 Basic 开启时有值。

| Backend / 类型 | 原生范围 | 可用字节指标 |
| --- | --- | --- |
| Standard Global | — | 无 |
| mimalloc Global | Process | 已提交、已预留 |
| mimalloc Heap | — | 无；仍可开启 Basic 请求计数 |
| jemalloc Global | Process | 已分配、常驻；需要 `config.stats` |
| jemalloc Heap | Memory | 已分配、常驻；需要 `config.stats` |

`BackendStatisticsScope::Process` 指原生 Backend 的汇总范围；`Memory` 指当前独立 Heap。mimalloc 汇总调用线程当前的原生 subprocess（通常为 main），不包含其他独立管理的原生 subprocess。后端没有的字段为 optional 空值。进程 RSS 不是请求字节，也不能由几个不同范围的指标相加得到。详细统计可能同步，应放在分配热路径之外。

查询前暂停相应范围内的分配、释放和线程清理：Memory 范围是当前 Heap，Process 范围是该 Backend，
也包含绕过 UniMemory 的原生调用。统一接口不保证原生快照可安全地与这些操作并发；
运行期间的并发监控使用 Basic `statistics()`。

mimalloc 3.4.3 的 malloc 计数受构建配置影响，且不能一致地表示当前在用字节；Heap 页指标记录在 subprocess 范围。
这些不可用指标返回 optional 空值，不以零代替。请求字节使用 Basic 计数；见[已核实映射](../backends/mimalloc.zh-CN.md)。

mimalloc 3.4.3 复制统计时没有保护整个快照的锁（[源码](https://github.com/microsoft/mimalloc/blob/v3.4.3/src/stats.c#L536)）。
这一共同契约不表示 jemalloc 的原生统计也缺少同步。

```cpp
std::optional<unimem::MemoryStatistics> requests = memory.statistics();
std::optional<unimem::BackendStatistics> details = memory.backend_statistics();
```
