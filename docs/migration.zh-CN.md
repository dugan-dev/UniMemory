# 纯头文件配置迁移

[目录](README.zh-CN.md) · [English](migration.md) · **简体中文**

`Memory` 仍为非模板类。包含 `<unimem/memory.h>` 并链接 `UniMemory::UniMemory`；该目标传递内联实现、生成配置和所选 SDK 依赖，不再生成 UniMemory 二进制库。

| 原用法 | 当前用法 |
| --- | --- |
| 可选后端启用选项、运行时选择后端 | CMake 显式设置 `UNIMEMORY_BACKEND=standard`、`mimalloc` 或 `jemalloc`，每次构建选择一个 |
| `global(backend)` / `heap(backend, statistics)` | `global()` / `heap(statistics)` |
| `capabilities(backend)` | `capabilities()` 查询配置后端的功能 |
| `supports(backend, option)` / `set_runtime_option(backend, option, value)` | `supports(option)` / `set_runtime_option(option, value)` |
| `available(backend)` / `BackendCapabilities::available` | 已删除，后端由 CMake 一次选定 |
| `Memory::selected_backend` / `selected_statistics` | 已删除，改用 CMake `UNIMEMORY_BACKEND` / `UNIMEMORY_STATISTICS` |
| `Memory::configure_global` | 已删除，改为 CMake `UNIMEMORY_STATISTICS=ON/OFF` |
| 首次获取 Global 前选择 Basic | ON 固定 Global Basic，OFF 固定 Global Disabled |
| 任意构建都可创建 Heap Basic | Basic 要求 ON；OFF 构建抛 `invalid_argument` |
| Global 默认 Standard | `global()` 使用 CMake 所选后端 |
| 分配前置条件始终检查 | `UNIMEMORY_CHECKS=AUTO`：Debug ON，其他配置 OFF；可显式指定 ON/OFF |

保留 `Backend` 与只读的 `memory.backend()` 查询；分配与功能查询直接使用固定构建配置。Heap 默认 Disabled；Stack 不受后端或统计选择影响。

每组后端、统计、检查策略使用独立的源码构建和安装目录。安装包导出固定配置，使用方继承它；各翻译单元不得混用配置或覆盖冲突宏。检查 OFF 不移除分配失败、类型化数量溢出、构造回滚及配置不匹配行为；指针、对齐、数量和生命周期仍须满足调用契约。

[构建选项](getting-started.zh-CN.md#构建选项) · [API](api-reference.zh-CN.md) · [生命周期](compatibility.zh-CN.md)
