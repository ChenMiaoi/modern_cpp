---
title: C++26
topic: cpp26
feature: overview
standard: C++26
status_checked_at: 2026-06-02
---

# C++26

> **状态说明**：本文按 2026-06-02 可公开资料整理。C++26 标准流程、提案接收状态和编译器支持变化很快。合并前需重新核对 WG21 draft、cppreference compiler support 和各编译器 release note。

## 已进入工作草案的特性

### Contracts（契约）

前置条件、后置条件和断言的语言级支持：

```cpp
int sqrt(int n)
    pre (n >= 0)
    post (r: r * r <= n)
{
    // ...
}
```

### 反射（Reflection）

编译期反射能力——在编译期查询类型信息、成员列表、枚举值等。这是 C++ 元编程的范式转变。（P2996R7 已被接受）

### 模式匹配（Pattern Matching）

`inspect` 表达式，类似 Rust 的 `match`，对 variant、optional 等类型尤其有用。

### `std::simd`

标准化的 SIMD 类型和操作，编写可移植的向量化代码。（P1928）

### Senders/Receivers

标准化的异步执行框架，比 `std::async` 更灵活、更可控。（P2300）

### `constexpr` 扩展

更多标准库函数可在编译期使用。

## 特性状态分类

| 状态 | 含义 | 示例 |
|------|------|------|
| 已进入工作草案 | 已被 C++26 接受 | 反射 (P2996)、Contracts |
| 已投票接受 | WG21 投票通过，等待合并 | Senders/Receivers (P2300) |
| 仍在演进 | 提案仍在修订中 | P3394（注解）、P3436 |
| 延后到 C++29 | 未赶上 C++26 时间线 | 部分 pattern matching 细节 |
| 仅实验实现 | 编译器实验分支支持 | 部分 constexpr 扩展 |

## 编译器支持状态

| 编译器 | 反射 | Contracts | std::simd | Notes |
|--------|------|-----------|-----------|-------|
| Clang/LLVM | 实验性 (`-freflection`) | 跟进中 | N/A | 参考实现 |
| GCC | 实验性分支 | 跟进中 | `<experimental/simd>` | GCC 14+ |
| MSVC | 跟进中 | 跟进中 | N/A | — |

## 状态说明

C++26 的特性仍在不断演化。本文档会跟踪最新进展，但内容可能滞后于标准委员会的最新决定。

参考来源：

- [C++ Reference](https://en.cppreference.com/)
- [ISO C++](https://isocpp.org/)
- [WG21 Papers](https://open-std.org/jtc1/sc22/wg21/docs/papers/)

## 延伸阅读

- [Contracts](/standards/cpp26/contracts)
- [Reflection](/standards/cpp26/reflection)
- [Pattern Matching](/standards/cpp26/pattern-matching)
