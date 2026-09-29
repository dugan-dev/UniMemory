# Runtime options

[目录](../README.zh-CN.md) · [English](runtime-options.md) · **简体中文**

调整 Backend 的**闲置内存回收延迟**。已释放的 Block 可能留在分配器缓存中，以加快下次分配；此选项控制闲置页归还系统的策略。

## 使用

```cpp
#include <unimem/memory.h>

using namespace unimem;
Backend backend = Backend::Mimalloc;
RuntimeOption option = RuntimeOption::UnusedPageReleaseDelayMs;
if (supports(backend, option)) {
    set_runtime_option(backend, option, 20);
}
Memory& memory = Memory::global(backend);
```

| 值 | 含义 |
| --- | --- |
| 正整数 | 建议等待时间，毫秒 |
| `0` | 请求尽快回收闲置页 |
| `-1` | 禁止定时回收 |
| 小于 `-1` | 无效，抛 `std::invalid_argument` |

## 使用规则

- 作用于 Backend 的默认策略，建议在创建线程和 Memory::heap() 前设置。
- Standard 不支持，返回 `false`。不同 Backend 对已有 Memory::heap() 的影响可能不同。
- 只回收符合条件的闲置页，**不会释放仍在使用的 Object**。
- 延迟不是完成期限，不保证 RSS 在指定时间下降。

更积极回收可能降低驻留内存，也可能增加分配开销。按实际负载测量。[Backend 参考](../backends/mimalloc.zh-CN.md) · [内存测试](../performance/memory.zh-CN.md)
