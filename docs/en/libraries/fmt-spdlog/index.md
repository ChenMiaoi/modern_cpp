---
title: "fmt / spdlog 源码级深度剖析"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# fmt / spdlog Source-Level Deep Analysis

> Source path: `references/impl/fmt/include/fmt/`, `references/impl/spdlog/include/spdlog/`

**fmt** is the de facto standard C++ formatting library, directly adopted by the C++20 standard as `std::format`. **spdlog** is the most popular logging library, built on top of fmt.

## Table of Contents

| Chapter | Source Path | Content |
|---------|------------|---------|
| [fmt Formatting Engine](/libraries/fmt-spdlog/fmt-engine) | `base.h`, `format.h` | Compile-time format string checking, type-erased tagged union, Dragonbox floating-point |
| [fmt Advanced Features](/libraries/fmt-spdlog/fmt-advanced) | `chrono.h`, `ranges.h`, `compile.h` | Custom types, compile-time formatting, chrono, ranges |
| [spdlog Architecture](/libraries/fmt-spdlog/spdlog) | `spdlog.h`, `logger.h`, `sinks/` | Logger/Sink/Formatter, async mode, log level pruning |
