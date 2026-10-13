# 统计

[目录](../README.zh-CN.md) · [English](statistics.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#快速开始)。

## 选择查询

| 查询 | 范围 | 内容 |
| --- | --- | --- |
| `statistics()` | 当前 Memory | 成功次数、请求字节、峰值 |
| Stack 的 `used()` | 当前 Buffer | 占用字节，包含填充和保留空间 |

## 统计配置

`UNIMEMORY_STATISTICS=ON` 允许 Global、Heap、Stack 开启 Basic；OFF 下配置 Basic 抛 `std::invalid_argument`。所有模式默认 Disabled。

所有构造函数接受 `StatisticsMode`，默认关闭。Global、Heap、Stack 的统计各自独立；Global 实例共用原生分配器。实例通过 `set_statistics(Basic)` 开启统计，Disabled 暂停；重新开启保留已有计数。

```sh
cmake -S . -B build/basic -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=ON
```

只统计通过当前 Memory 的成功请求，不包含直接调用后端的分配。Heap、Stack reset 后重新计数；开启统计有计数开销。

| 字段 | 含义 |
| --- | --- |
| `allocations` / `deallocations` / `reallocations` | 成功操作次数 |
| `live_bytes` | 尚未释放的请求字节 |
| `peak_live_bytes` | 请求字节峰值 |

## 查询时机

`statistics()` 返回五字段值，Disabled 时全部为零。同一 Memory 的快照在同一时刻读取。请求字节与后端原生指标、进程 RSS 含义不同，不能相加；原生诊断请直接使用后端工具。

[后端指标说明](../allocator-capabilities.zh-CN.md) · [生命周期与线程](../compatibility.zh-CN.md)

`reset_statistics()` 清除五个字段并开始新的计数周期；不释放存储。

重置后，只计入新周期中的请求；释放重置前的分配不会造成 live 字节下溢。
