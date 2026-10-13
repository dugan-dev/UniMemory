# 统计

[首页](../README.zh-CN.md) · [English](statistics.md)

`statistics()` 返回单个 Global、Heap 或 Stack 的请求计数。各实例账本独立；不同 Global 共享原生分配器，但计数独立。计数不包含直接原生调用、后端开销或进程 RSS。

## 启用计数

所有构造函数默认 `StatisticsMode::Disabled`。`UNIMEMORY_STATISTICS=ON` 允许 `StatisticsMode::Basic`；OFF 构建中构造或配置 Basic 抛 `std::invalid_argument`。

```sh
cmake -S . -B build/basic -DUNIMEMORY_BACKEND=standard -DUNIMEMORY_STATISTICS=ON
```

使用 C++20，包含 `<unimem/all.h>`，链接 `UniMemory::UniMemory`。以下示例要求 ON 配置：

```cpp
#include <unimem/all.h>
#include <cassert>

int main() {
    unimem::Global memory(unimem::StatisticsMode::Basic);
    {
        auto block = memory.make_block(64);
        const auto during = memory.statistics();
        assert(during.allocations == 1 && during.live_bytes == 64);
    }
    const auto after = memory.statistics();
    assert(after.deallocations == 1 && after.live_bytes == 0);
    assert(after.peak_live_bytes == 64);
    memory.reset_statistics(); // Memory 操作处于空闲状态
    assert(memory.statistics().allocations == 0);
}
```

## 五个字段

所有字段类型为 `std::uint64_t`。

| 字段 | 含义 |
| --- | --- |
| `allocations` | 成功分配请求数 |
| `deallocations` | 成功释放请求数 |
| `reallocations` | 成功调整大小请求数 |
| `live_bytes` | 已计数、尚未释放的请求字节 |
| `peak_live_bytes` | 已计数活跃请求字节峰值 |

失败请求不增加计数。零字节分配及空指针释放无操作；重新分配空指针走分配路径，调整至零走释放路径。计数也覆盖此 Memory 的所有者及容器适配器。共享指针对象/控制块布局由标准库决定，不要假设 make_shared 的固定分配次数。

## 查询、暂停与清空

`statistics()` 返回当前实例的一致快照。关闭统计时，五个字段均为零；开启统计会增加开销。

`set_statistics(mode)` 或 `reset_statistics()` 前暂停 Memory 操作；即使 Global/Heap 支持并发分配/释放，配置也要求实例空闲。关闭期间不记录新增分配；此前已计数的分配释放后，存活字节仍会减少。重新开启保留历史，不补记关闭期间的分配。

`reset_statistics()` 清空五字段并开启新统计周期，不释放存储。重置前分配不计入新周期活跃字节，释放它们不会下溢。Heap/Stack reset 释放或重用存储并清空计数。Stack rewind 调整回收存储的活跃字节，保留操作次数和历史峰值。

## 解读内存占用

Stack used 报告消耗缓冲区字节，包括填充和逐次释放后保留的存储；capacity 为固定缓冲区大小。所有者销毁可降低 live_bytes，但 used 保持不变。

请求计数、原生分配器指标及进程 RSS 衡量不同量，不能相加。UniMemory 不提供原生统计快照 API。使用后端工具诊断原生指标，并在其 Memory/进程范围协调活动，包括原生调用及线程清理。Global/Heap 支持并发分配/释放；Stack 仍仅单线程。应用对象访问的同步独立于统计快照。
