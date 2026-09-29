# 统计

[目录](../README.zh-CN.md) · [English](statistics.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#初始化与统计)。

## 选择查询

| 查询 | 范围 | 内容 |
| --- | --- | --- |
| `statistics()` | 当前 Memory | 成功次数、请求字节、峰值 |
| `backend_statistics()` | Backend 或独立 Heap | 后端可用的原生指标 |
| Stack 的 `used()` | 当前 Buffer | 占用字节，包含填充和保留空间 |

## 统计配置

默认 Disabled。启用 Basic：Global 在首次 `global()` 前配置，Heap 在创建时传入；初始化后改模式抛 `logic_error`，重复设置同值允许。

只统计通过当前 Memory 的成功请求，不包含直接调用后端的分配。Heap reset 后重新计数；开启统计有计数开销。

| 字段 | 含义 |
| --- | --- |
| `allocations` / `deallocations` / `reallocations` | 成功操作次数 |
| `live_bytes` | 尚未释放的请求字节 |
| `peak_live_bytes` | 请求字节峰值 |

## 后端详细统计

先查询实例的 `memory.capabilities().detailed_statistics`。Stack 两种统计均返回空值；`statistics()` 仅在 Basic 开启时有值。

| Backend / 类型 | 原生范围 | 可用字节指标 |
| --- | --- | --- |
| Standard Global | — | 无 |
| mimalloc Global | Process | 已提交、已预留 |
| mimalloc Heap | — | 无；仍可开启 Basic 请求计数 |
| jemalloc Global | Process | 已分配、常驻；需要 `config.stats` |
| jemalloc Heap | Memory | 已分配、常驻；需要 `config.stats` |

| `scope` | 含义 |
| --- | --- |
| `Memory` | 当前独立堆 |
| `Process` | 该后端的原生汇总范围，可能包含直接调用后端的分配 |

mimalloc 不汇总其他独立管理的原生 subprocess。不可用字段为空值，不以零代替。请求字节、原生内存指标和进程 RSS 含义不同，不能相加。

## 查询时机

- 并发监控使用 Basic `statistics()`；快照中的字段可能来自不同时刻。
- 原生统计适合诊断阶段，避免每次分配都查询。
- 查询原生统计前，暂停相应 Memory/Process 范围内的分配、释放和线程清理，包括直接调用后端的操作。

[后端指标说明](../allocator-capabilities.zh-CN.md) · [生命周期与线程](../compatibility.zh-CN.md)
