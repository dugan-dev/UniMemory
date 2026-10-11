# 纯头文件性能控制

[报告](../performance.zh-CN.md) · [English](header-only.md) · **简体中文**

测量日期 **2026-10-11**。Windows x64 / MSVC19.44.35208，mimalloc3.4.3 动态 SDK，Release `/O2 /EHsc /MD`，统计 OFF、检查 OFF。核心保留完整 Heap/Stack 接口、Global PMR 与私有无状态 Global 共享指针分配器。三组构建使用同一源码、SDK 二进制和 CRT；每组 SDK/API 基准翻译单元使用相同编译选项，SDK 生产二进制没有重编。

V5 协议按 compact 方式固定工作线程 CPU，并检查负载前后 CPU 一致；五组配对交替先后顺序，校准后双方使用相同循环次数，并验证合法数据及校验工作。保留全部 **23 个负载/线程规模组**，包括原始及类型化操作、PMR、1/2/4/8/16 线程分配、最高无状态 shared 参考与独立有状态 shared 参考。这是有限微基准，不保证所有应用或单次操作延迟。

## Vector：绝对时间与配对比值

场景 reserve256个 uint64 元素，构造相同256个值，读取校验和并销毁容器。ns/轮表示完整容器工作，不是 SDK malloc 延迟。比值为五组配对 `100 * SDK/API` 的中位数；独立计算的两侧时间中位数相除不一定等于该值。

| 控制 | SDK 中位 ns | API 中位 ns | 配对比值中位 [最小,最大] | 组中位数 >=90% | 每pair均 >=90%的组 |
| --- | ---: | ---: | --- | --- | --- |
| Default `/Ob2`, IPO OFF | 221.59 | 250.05 | 89.79% [84.30,94.31] | 22/23 | 20/23 |
| `/Ob2`, IPO ON | 214.45 | 492.48 | 43.02% [42.90,44.44] | 22/23 | 19/23 |
| `/Ob3`, IPO OFF | 252.45 | 210.34 | 119.02% [115.90,129.51] | 23/23 | 23/23 |

相对默认配置，`/Ob3` 使 API vector 绝对时间改善约 **16%**，同时 SDK vector 负载变慢约 **14%**。119.02% 包含双方变化，不能说 SDK 分配器提速；API210.34ns 也比默认 SDK221.59ns 约低5%。IPO 则使 API 从250.05增至492.48ns，属于实际退化，不推荐为默认策略。

`/Ob3` 全23组中位数和每个已记录 pair 均达到该协议的 >=90% 验收线：最低中位98.299%，最低单pair92.574%，最高中位121.726%。这不是“与原生机器码完全一样”或所有应用都最快的证明。

## 机器码确认的事实

默认配置将 API 冷扩容内联、SDK 扩容移出函数。IPO 将 API `reserve` 移为独立 helper：`0x140008f34 -> reserve 0x140009fe0 -> AllocateAtLeast 0x14000a1c0`。分配 helper 重新读取通用上下文和数量，检查零/溢出，并在`0x14000a229`比较 Global 身份后进入相同 `mi_malloc` SDK入口。这是正常 reserve 的额外工作；成功 reserve256 后的扩容仍为冷路径。

IPO API 成功 push 每元素11条解码指令，默认15条，因此不能按完整函数条数或将所有冷指令视为每push执行来解释退化。helper及内存读写形态变化不足以定量解释全部差距，周期级归因仍未知。编译器可能分别改变 SDK/API 的容器循环。

## Shared 分配与语义

在本次 MSVC、64字节标量 payload 下，最高 shared 参考实际请求 **SDK80字节 / API80字节**，alignment8。Global `make_shared()` 使用私有空分配器，以永生 Global隐式绑定；Heap/Stack 保留公共分配器的明确 Memory绑定。统计 ON仍记录精确成功分配/释放次数，构造失败仍由标准分配器清理回滚。

保留的 `shape_matched_shared` 组是原有有状态 SDK参考：其分配器8字节绑定字段匹配公共 `Allocator`，但最终 Global shared路径已使用私有空分配器。该参考请求88字节，并非当前 API80字节的同尺寸参考；它与最高无状态参考完整并列报告，不用于替代主参考。

## 构建策略与证据范围

`UNIMEMORY_AGGRESSIVE_INLINING=ON` 通过导出接口目标选择 MSVC Release `/Ob3`，影响使用方整个 C++翻译单元；源码及安装消费者可设 OFF。GCC/Clang选项不变，不自动开启全局IPO。Microsoft说明 `/Ob3` 从 Visual Studio2019起提供更积极的内联，但标记与选项仍是建议，不保证内联。[官方 `/Ob` 说明](https://learn.microsoft.com/en-us/cpp/build/reference/ob-inline-function-expansion?view=msvc-170)。

此前云端 source35共207个单元，其中197个中位数达到90%、十个失败。最终 source39已经变化，本次 Windows/mimalloc/OFF不能证明旧云端失败已修复。其他 SDK、统计ON、编译器、链接形式、应用与后续每pair仍须独立验收。README原性能图继续保留为历史测量。

## Standard 后端：可抛异常分配控制

独立控制测于 **2026-10-11**，使用本地 Xeon / WSL2、GCC13.3 Release，固定 CPU0。两组保持相同基准源码、编译选项、C++运行时以及直接调用可抛异常 aligned `operator new` 的原生参考。五组进程配对，每轮200,000次操作、九轮，alignment16，统计 OFF、检查 OFF。

唯一分配因素是移除私有 Global Standard 的 `std::nothrow` 包装，使用对应可抛异常分配并传播 `std::bad_alloc`。条件 `noexcept` 保留 mimalloc/jemalloc helper 原有约定。零值处理、重分配失败后的所有权、构造回滚和精确 Basic统计仍须保持；Heap/Stack 路径不变。

| 字节 | 原 API 中位 ns | 可抛异常 API 中位 ns | 原配对比值中位 | 可抛异常配对比值中位 [最小,最大] |
| --- | ---: | ---: | ---: | --- |
| 16 | 13.1275 | 10.8337 | 77.11% | 102.53% [79.97,110.35] |
| 64 | 12.7759 | 10.7696 | 78.39% | 99.07% [89.62,108.61] |
| 256 | 12.8916 | 11.2464 | 85.00% | 99.41% [91.33,108.75] |
| 4096 | 22.4839 | 23.8641 | 85.50% | 98.02% [84.91,108.85] |
| 65536 | 24.0164 | 22.8747 | 86.39% | 97.90% [94.79,120.54] |

五个配对中位数均超过90%，但仍有单pair低于90%。比值不能证明每种尺寸的绝对时间改善：本次独立计算的4096字节 API时间中位数反而增加。强化后的 Standard分配/OOM套件在四个原型配置全部通过（Release ON673、OFF671；Debug ON676、OFF674例）。这是独立本地主机、两构建控制，不能撤销云端交付失败，也不能证明其他后端或平台的性能。

[全部配对测量](../results/header-only/linux-standard-throw/measurements.csv) · [环境](../results/header-only/linux-standard-throw/environment.json)

## 原始数据

| 配置 | 全23组汇总 | 全部配对路径测量 |
| --- | --- | --- |
| 默认 | [汇总](../results/header-only/windows-mimalloc-default/summary.csv) | [测量](../results/header-only/windows-mimalloc-default/measurements.csv) |
| IPO | [汇总](../results/header-only/windows-mimalloc-ipo/summary.csv) | [测量](../results/header-only/windows-mimalloc-ipo/measurements.csv) |
| Ob3 | [汇总](../results/header-only/windows-mimalloc-ob3/summary.csv) | [测量](../results/header-only/windows-mimalloc-ob3/measurements.csv) |

[构建选项](../getting-started.zh-CN.md#构建选项) · [生命周期](../compatibility.zh-CN.md)
