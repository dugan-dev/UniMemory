# 验证

[文档](README.zh-CN.md) · [English](testing.md) · **简体中文**

CI 配置覆盖纯头文件库的 Windows、Linux、macOS 验证。每个源码修订的结果见[最新 Actions](https://github.com/dugan-dev/UniMemory/actions?query=branch%3Adev)。

| 范围 | 检查内容 |
| --- | --- |
| 内存分配 | 零大小、对齐、溢出、扩容和失败回滚 |
| 对象与所有权 | 构造、析构、数组、共享/弱引用及跨线程释放 |
| 标准容器 | Allocator、PMR、复制、移动、嵌套容器和 PMR 零大小请求 |
| 堆与栈 | 归属、回收/重置、容量、标记与回退 |
| 并发 | Global 构造、线程间交接和精确请求计数 |
| 项目接入 | 单入口头文件、六种包含顺序、多翻译单元、安装配置和 PIC 选择 |
| 错误检测 | ASan、遇错终止的 UBSan、TSan、静态分析与覆盖率 |

核心测试覆盖 Debug/Release 与统计开关，可选后端分别验证。消毒器检查已执行路径；覆盖率报告展示尚未执行的行与分支。

```sh
cmake --preset release -DUNIMEMORY_BUILD_EXAMPLES=ON
cmake --build --preset release
ctest --preset release
```

[构建选项](getting-started.zh-CN.md#构建选项) · [SDK 接入](sdk-integration.md) · [性能](performance.zh-CN.md)
