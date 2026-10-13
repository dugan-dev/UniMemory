# 运行时选项

[目录](../README.zh-CN.md) · [English](runtime-options.md) · **简体中文**

示例见[快速开始](../../README.zh-CN.md#快速开始)。

`Memory::config(release_delay_ms)` 调整 CMake 所选后端的闲置内存归还等待时间。分配器可能缓存已释放空间以供复用。

## 选项值

| 值 | 含义 |
| --- | --- |
| 正整数 | 建议等待时间，毫秒 |
| `0` | 请求尽快回收闲置页 |
| `-1` | 禁止定时回收 |
| 小于 `-1` 或超出 `long` 范围 | 无效，抛 `std::invalid_argument` |

## 使用规则

- 作用于所选后端的默认策略，建议在创建线程和 Heap() 前设置。
- Standard 对合法值不执行操作。jemalloc 更新已初始化 arena 的延迟，并设置新 arena 的默认值。
- 只回收符合条件的闲置页，**不会释放仍在使用的 Object**。
- 延迟不是完成期限，不保证 RSS 在指定时间下降。
- 后端拒绝设置时抛 `std::runtime_error`。

更积极回收可能降低驻留内存，也可能增加分配开销。按实际负载测量。[Backend 参考](../backends/mimalloc.zh-CN.md) · [内存测试](../performance/memory.zh-CN.md)

`Memory::config(release_delay_ms)` 只设置回收延迟，不影响实例统计。
