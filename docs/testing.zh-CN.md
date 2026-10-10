# 测试

[目录](README.zh-CN.md) · [English](testing.md) · **简体中文**

## 远程验收 · 2026-10-10

扩展验收对应源码 `ca4919f`。[原生矩阵](remote-validation.md#platform-and-compiler-matrix)
覆盖 26 项 Debug/Release 编译器配置，包括要求的六种平台/架构组合，并验证安装消费者。

| 证据 | 范围 |
| --- | --- |
| [构建](https://github.com/dugan-dev/UniMemory/actions/runs/38027100987) | Standard、可选后端、共享库、示例、安装与文档 |
| [平台矩阵](https://github.com/dugan-dev/UniMemory/actions/runs/38027100962) | 实际编译器版本、二进制架构、Debug/Release |
| [错误检测](https://github.com/dugan-dev/UniMemory/actions/runs/38027100952) | 严格 UBSan 探针、封装/原生检测、静态分析与覆盖率 |
| [发布验证](https://github.com/dugan-dev/UniMemory/actions/runs/38027100940) | 现有原生套件、错误检测与压力验证 |
| [性能](https://github.com/dugan-dev/UniMemory/actions/runs/38027100953) | 三平台完整测量、报告回归测试与图表自动发布 |

GCC 覆盖率配置执行 1,673 项注册测试。已插桩项目代码覆盖 **604/635 行（95.12%）**、
**337/475 分支（70.95%）**，包含未执行的编译单元。该结果对应具体配置，不代表所有
模板实例和路径均已覆盖；源码报告列出剩余的分配失败、能力判断等分支。

Clang TSan 执行 1,671 项；两项与其全局 new 替换不兼容的失败注入仍在普通构建和兼容
ASan 配置中要求通过。jemalloc 内部仍未插桩。[检测范围与压力规模](remote-validation.md#detection-and-pressure)

报告回归测试先在 `568cc2b` 远程复现末尾空行与统计柱形难以区分的问题；修复后
`6738c77` 的 17 项工具测试全部通过，覆盖首次/不同基线、场景缺失拒绝和 SVG 有效性。
最终工具套件共 23 项全部通过，包含发布身份、文件范围和完整分页检查。
[图表 PR #5](https://github.com/dugan-dev/UniMemory/pull/5) 的五项真实 PR 验收成功后才自动合并。
18 张 SVG 均标识测量源码 `ca4919f`，未使用手动派发检查代替 PR 验收。

## 2026-10-08 至 2026-10-09 审查修复验收

| 配置 | 通过 / 注册项 | 安装消费者 |
| --- | --- | --- |
| Windows / MSVC，静态，三后端，Release | **1666/1666** | **7/7** |
| Windows / MSVC，静态，三后端，Debug | **1666/1666** | **7/7** |
| Windows / MSVC，共享，三后端，Release | **1664/1664** | **7/7** |
| Linux / GCC，静态，三后端，Release | **1667/1667** | **7/7** |
| Linux / GCC，共享，三后端，Release | **1666/1666** | **7/7** |
| Linux / Clang，Standard，ASan + UBSan + 泄漏检测 | **715/715** | **7/7** |

全部零失败。数量包含参数化、包发现、头文件顺序和压力测试，重复配置不算新增功能。
共享库中依赖 executable-level allocation interception 的测试数量不同。
GitHub 于 2026-10-09 验证修复提交 `65c4a46`：全部 13 项
[跨平台构建任务](https://github.com/dugan-dev/UniMemory/actions/runs/37871859097)和全部 5 项
[发布验收任务](https://github.com/dugan-dev/UniMemory/actions/runs/37871859066)通过，包含 macOS、ASan 和 ThreadSanitizer。
下表的旧数值仍为历史结果。
[审查结论与接口约束](review-2026-10-08.md)

后续文档修订 `08d97f0` 也通过全部 13 项[构建任务](https://github.com/dugan-dev/UniMemory/actions/runs/37873043811)。各链接只验证该任务记录的修订，不代表更早的 `v0.0.1` 发布或此后每个 main 提交都已通过。

## 2026-09-28 的 0.0.1 历史验证

| 构建 | 结果 |
| --- | --- |
| Windows x64 / MSVC，三种 Backend | Release **1626/1626**，Debug **1626/1626** |
| Linux x64 / GCC / WSL，三种 Backend | **1627/1627** |
| macOS / Apple Clang，三种 Backend | GitHub CI **1626/1626**；安装包使用程序通过 |
| Linux Standard / Stack，ASan + UBSan | **681/681**，未发现 Sanitizer 错误 |
| Linux Standard / Stack，ThreadSanitizer | **678/678**，未发现竞争；关闭示例 |
| Windows、Linux 安装包 | 包查找、版本检查、程序编译运行通过 |
| 独立 GitHub 克隆 | README 构建、Standard **681/681**；静态/共享安装包使用各 **2/2** |
| 文档示例 | 18 个 README 示例 + 2 个 PMR 程序；Linux 单后端与三后端构建，**40/40** 次运行通过 |

Backend 版本：mimalloc 3.4.3、jemalloc 5.3.1。以上验证针对统一 Memory 接口。

## 覆盖范围

| 主题 | 验证内容 |
| --- | --- |
| 分配与扩容 | 零大小、对齐、溢出、内容保留、清零增长、失败回滚 |
| 对象与智能指针 | 构造、析构、数组、接管、共享与弱引用 |
| 容器 | Allocator、PMR、复制、移动、交换 |
| 堆与栈 | 归属、整体释放、标记回退、容量不足 |
| 并发与统计 | 多线程分配释放、实例获取、请求计数 |
| 压力测试 | 大量循环分配、随机操作、所有权交接 |

错误配对、失效指针、reset 后继续使用、无同步的对象写入仍由调用方避免。Shared Array 的标准库限制见[兼容性指南](compatibility.zh-CN.md#标准库行为)。

## 运行

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

默认启用 Standard；可选 Backend 需要开启构建选项并提供依赖。[配置方法](guides/backends.zh-CN.md)

## 自动化验证

[跨平台构建](https://github.com/dugan-dev/UniMemory/actions/workflows/ci.yml)检查平台、后端和安装包；[发布验证](https://github.com/dugan-dev/UniMemory/actions/workflows/release-validation.yml)检查 Sanitizer、压力测试和上游测试。

Android/iOS 尚未设备验证。上述结果仅适用于列出的配置与场景。

[详细验证记录](upstream-validation.md) · [性能对比](performance.zh-CN.md)
