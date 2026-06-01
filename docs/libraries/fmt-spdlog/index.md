# fmt / spdlog 源码级深度剖析

> 源码路径：`references/impl/fmt/include/fmt/`, `references/impl/spdlog/include/spdlog/`

**fmt** 是 C++ 格式化库的事实标准，直接被 C++20 标准采纳为 `std::format`。**spdlog** 是最流行的日志库，基于 fmt 构建。

## 目录

| 章节 | 源码路径 | 内容 |
|------|---------|------|
| [fmt 格式化引擎](/libraries/fmt-spdlog/fmt-engine) | `base.h`, `format.h` | 编译期格式串检查、类型擦除 tagged union、Dragonbox 浮点 |
| [fmt 高级特性](/libraries/fmt-spdlog/fmt-advanced) | `chrono.h`, `ranges.h`, `compile.h` | 自定义类型、编译期格式化、chrono、ranges |
| [spdlog 架构](/libraries/fmt-spdlog/spdlog) | `spdlog.h`, `logger.h`, `sinks/` | Logger/Sink/Formatter、异步模式、日志级别裁剪 |
