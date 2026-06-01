# C++26

C++26 是当前正在标准化中的版本（预计 2026 年发布），已确认包含多项重量级特性。

## 已确认 / 进行中的特性

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

编译期反射能力——在编译期查询类型信息、成员列表、枚举值等。这是 C++ 元编程的范式转变。

### 模式匹配（Pattern Matching）

`inspect` 表达式，类似 Rust 的 `match`，对 variant、optional 等类型尤其有用。

### `std::simd`

标准化的 SIMD 类型和操作，编写可移植的向量化代码。

### Senders/Receivers

标准化的异步执行框架，比 `std::async` 更灵活、更可控。

### `constexpr` 扩展

更多标准库函数可在编译期使用。

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
