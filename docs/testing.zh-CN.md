# 测试

[目录](README.zh-CN.md) · [English](testing.md) · **简体中文**

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
本次修订尚未在 macOS 或 ThreadSanitizer 验证，下表为历史结果。
[审查结论与接口约束](review-2026-10-08.md)

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
