# 测试

[目录](README.zh-CN.md) · [English](testing.md) · **简体中文**

## 0.0.1 验证

| 构建 | 结果 |
| --- | --- |
| Windows x64 / MSVC，三种 Backend | Release **1626/1626**，Debug **1626/1626** |
| Linux x64 / GCC / WSL，三种 Backend | **1627/1627** |
| Linux Standard / Stack，ASan + UBSan | **681/681**，未发现 Sanitizer 错误 |
| Linux Standard / Stack，ThreadSanitizer | **678/678**，未发现竞争；关闭示例 |
| Linux Standard 链接 Google TCMalloc | **671/671** 项适配测试 |
| Windows、Linux 安装包 | 精确识别 `0.0.1`，版本宏与使用程序通过 |
| 独立 GitHub 克隆 | README 构建、Standard **681/681**；静态/共享安装包使用各 **2/2** |
| 文档示例 | 31 个片段编译通过；Standard 路径运行通过 |

Backend 版本：mimalloc 3.4.3、jemalloc 5.3.1。以上验证针对统一 Memory 接口。

## 覆盖范围

| 主题 | 检查内容 |
| --- | --- |
| 分配 | 零大小、对齐、溢出、分配失败 |
| Object / Array | 构造转发、原类型析构、逆序回滚、接管、Shared/Weak/别名所有权 |
| Container | Allocator rebind/传递、PMR、复制/移动/交换、不同 Memory |
| 扩容 | 保留内容、清零新增范围、失败保留原内存 |
| Heap / Stack | 归属、独占控制、重置失败回滚、外来/失效/嵌套 Mark、构造失败 |
| Global / Threads | 并发配置/获取、共享实例、静态析构、同步的 Object 读写与所有权交接 |
| Statistics | 启动配置、并发准确总数、原生字段不可用、大块释放计数回归 |
| 压力测试 | 每种 Backend 240 个循环场景 + Stack 240 个场景；352 个所有权随机序列 × 1024 步、192 个分配随机序列 × 4096 步、三次 250000 步长循环 |
| 错误用法 | 非空 Deleter 未绑定 Memory、Array 数量为零、失效 Mark、溢出、不支持的能力 |

414 项接口使用测试覆盖 Global/Heap、统计开关和 Stack。
并发测试采用正确同步；任意非法指针、无同步 Object 写入、reset 后继续访问仍属于调用方错误。
Shared Array 异常析构顺序存在已复现的 libstdc++ 13 偏差，
[兼容性指南](compatibility.zh-CN.md#标准库行为)说明对应测试条件。

## 运行

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

默认启用 Standard；可选 Backend 需要开启构建选项并提供依赖。[配置方法](guides/backends.zh-CN.md)

## 原生验证

[上游测试与复现](upstream-validation.md) · [性能报告](performance.zh-CN.md)

当前构建未验证 macOS 和 Android/iOS。测试结果仅代表已检查的配置及负载。

WSL 的 ThreadSanitizer 使用 `setarch x86_64 -R`，仅为测试进程及子进程关闭地址随机化，避免启动映射错误；
未修改系统配置。此次 Standard/Stack 检查不包含原生分配器内部的 Sanitizer 插桩。

上述结果来自本地验证。仓库工作流提供可复现的 CI 配置。
