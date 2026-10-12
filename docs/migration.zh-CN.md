# 纯头文件配置迁移

[目录](README.zh-CN.md) · [English](migration.md) · **简体中文**

`Memory` 为非模板基类，统一实现非虚分配接口；`Global`、`Heap`、`Stack` 为 final 派生类。包含 `<unimem/all.h>` 并链接 `UniMemory::UniMemory`；该目标传递内联实现、生成配置和所选 SDK 依赖，不再生成 UniMemory 二进制库。

`Memory heap = Memory::heap()` 改为 `Heap heap;` 或 `auto heap = Memory::heap()`。缓冲区使用 `Stack scratch(buffer);` 与 `Stack::Mark`。通用分配及所有权代码可继续使用 `Memory&`；Heap/Stack 管理接口需使用对应具体类型。

使用具体类型管理 Heap、Stack 生命周期，例如 `std::unique_ptr<Heap>`；`Memory&`、`Memory*` 用作借用视图。禁止通过 `Memory*` 删除对象。

| 原用法 | 当前用法 |
| --- | --- |
| `<unimem/memory.h>` 作为完整入口 | `<unimem/all.h>` 汇总全部声明与内联实现；`memory.h` 仅声明 `Memory`，`global.h`、`heap.h`、`stack.h` 分别声明对应类 |
| 可选后端启用选项、运行时选择后端 | CMake 显式设置 `UNIMEMORY_BACKEND=standard`、`mimalloc` 或 `jemalloc`，每次构建选择一个 |
| `backend_statistics()` / `BackendStatistics` | 已移除；`statistics()` 仅提供五个请求计数字段 |
| `global(backend)` / `heap(backend, statistics)` | `Memory::global(statistics)` / `Heap(statistics)` |
| `capabilities(...)` / `memory.capabilities()` | 已移除；按 Memory 模式和所选 Backend 使用接口 |
| `supports(...)` / `set_runtime_option(...)` | `Memory::config(release_delay_ms)`；Standard 合法值不执行操作 |
| `available(backend)` / `BackendCapabilities::available` | 已删除，后端由 CMake 一次选定 |
| `Memory::selected_backend` / `selected_statistics` | 已删除，改用 CMake `UNIMEMORY_BACKEND` / `UNIMEMORY_STATISTICS` |
| `Memory::configure_global` | 已删除，改为 CMake `UNIMEMORY_STATISTICS=ON/OFF` |
| 首次获取 Global 前选择 Basic | 首次获取 Global 时传入 Basic；ON 允许 Basic，OFF 拒绝 Basic |
| 任意构建都可创建 Heap Basic | Basic 要求 ON；OFF 构建抛 `invalid_argument` |
| Global 默认 Standard | `global()` 使用 CMake 所选后端 |
| 分配前置条件始终检查 | `UNIMEMORY_CHECKS=AUTO`：Debug ON，其他配置 OFF；可显式指定 ON/OFF |

公共后端标识与查询已移除。后端由 CMake 选择；在首次获取 Global 或构造 Heap/Stack 时传入统计模式，所有模式默认 Disabled。

每组后端、统计、检查策略使用独立的源码构建和安装目录。安装包导出固定配置，使用方继承它；各翻译单元不得混用配置或覆盖冲突宏。检查 OFF 不移除分配失败、类型化数量溢出、构造回滚及配置不匹配行为；指针、对齐、数量和生命周期仍须满足调用契约。

[构建选项](getting-started.zh-CN.md#构建选项) · [API](api-reference.zh-CN.md) · [生命周期](compatibility.zh-CN.md)
