---
title: "C++17"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++17

C++17（ISO/IEC 14882:2017）是一次大规模更新，引入了大量实用性极强的语言特性和库组件，进一步提升了 C++ 的表达力和安全性。

## 核心特性

### 语言层面

| 特性 | 说明 |
|------|------|
| 结构化绑定 | `auto [x, y] = pair;` |
| `if constexpr` | 编译期分支，替代 SFINAE 黑魔法 |
| 折叠表达式 | `(args + ...)` 简化可变参数模板 |
| 类模板参数推导 (CTAD) | `pair p(1, 2.0);` 无需显式模板参数 |
| 内联变量 | 头文件中定义全局变量的正确方式 |
| 嵌套命名空间 | `namespace A::B::C {}` |
| `[[nodiscard]]` / `[[maybe_unused]]` / `[[fallthrough]]` | 更多属性 |

### 标准库层面

| 组件 | 说明 |
|------|------|
| `std::optional` | 可能为空的值语义容器 |
| `std::variant` | 类型安全的 union |
| `std::any` | 任意类型的容器 |
| `std::string_view` | 非拥有的字符串视图，零拷贝 |
| `<filesystem>` | 文件系统操作标准化 |
| 并行算法 | `std::execution::par` 执行策略 |
| `std::apply` | 将函数应用到 tuple 元组 |
| `std::byte` | 类型安全的字节类型 |
| `std::clamp` | 数值钳制 |
| `std::invoke` | 统一调用语法 |
| `std::from_chars` / `std::to_chars` | 高性能数值转换 |
| `std::conjunction` / `disjunction` / `negation` | 类型 traits 逻辑运算 |

## 设计哲学

C++17 的设计哲学可以总结为"实用优先"：

- **消除样板代码**：结构化绑定、CTAD 减少冗余声明
- **零开销抽象**：`string_view`、`optional` 不引入运行时开销
- **编译期能力**：`if constexpr` 让模板代码更可读
- **安全性**：`[[nodiscard]]` 帮助捕获被忽略的返回值

## 编译器支持

| 编译器 | 完整支持版本 |
|--------|-------------|
| GCC | 9+ |
| Clang | 8+ |
| MSVC | VS 2019 (16.0)+ |

## 延伸阅读

- [结构化绑定](/standards/cpp17/structured-bindings)
- [std::optional](/standards/cpp17/optional)
- [std::string_view](/standards/cpp17/string-view)
