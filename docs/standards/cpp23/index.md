---
title: "C++23"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++23

C++23（ISO/IEC 14882:2024）延续了 C++20 的方向，在四大基石之上进一步完善和扩展。

## 核心特性

| 特性 | 说明 |
|------|------|
| `std::expected<T,E>` | 带错误类型的返回值，替代异常 |
| `std::print` / `std::println` | 替代 `iostream` 的现代打印方式 |
| `std::mdspan` | 多维数组视图 |
| `std::generator` | 协程生成器的标准实现 |
| `deducing this` | 显式对象参数，消除 CRTP 和重复代码 |
| `if consteval` | 编译期与运行期的条件分支 |
| 多维下标运算符 | `matrix[i, j]` 语法 |
| `std::flat_map` / `flat_set` | 基于排序 vector 的容器 |
| `import std` | 导入整个标准库 |

## 亮点解读

### `deducing this`

```cpp
struct Widget {
    // C++20 需要写两份（const 和非 const）
    // C++23 一份搞定
    template<typename Self>
    auto&& name(this Self&& self) { return std::forward<Self>(self).name_; }
};
```

### `std::print`

```cpp
// 不再需要 <iostream>，比 printf 类型安全
std::print("Hello, {}! pi = {:.2f}\n", "world", 3.14159);
```

### `import std`

```cpp
// 一行搞定，替代所有 #include
import std;
```

## 编译器支持

| 编译器 | 支持状态 |
|--------|---------|
| GCC | 14+（部分特性） |
| Clang | 18+（部分特性） |
| MSVC | VS 2022 17.8+（部分特性） |

## 延伸阅读

- [std::expected](/standards/cpp23/expected)
- [std::print](/standards/cpp23/print)
- [Deducing this](/standards/cpp23/deducing-this)
