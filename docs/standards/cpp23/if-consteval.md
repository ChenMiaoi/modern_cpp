---
title: "if consteval"
topic: unknown
feature: if-consteval
standard: N/A
status_checked_at: 2026-06-02
---
# if consteval

C++23 引入 `if consteval`，允许在 constexpr 函数中显式区分编译期和运行时执行路径，取代了 C++20 中的 `std::is_constant_evaluated()`。

## 基本语法

```cpp
constexpr int compute(int n) {
    if consteval {
        // 编译期路径：所有操作必须是 constexpr 合法的
        return n * n;
    } else {
        // 运行时路径：可使用非 constexpr 操作
        return n * n;
    }
}
```

## 与 if constexpr 的区别

```cpp
// if constexpr — 模板实例化时丢弃分支
template <typename T>
void f(T t) {
    if constexpr (std::is_integral_v<T>) { /* ... */ }
    else { /* ... */ }
}

// if consteval — 每次调用时判断上下文
constexpr int g(int n) {
    if consteval { return n * 2; }   // 编译期走这里
    else { return n * 3; }           // 运行时走这里
}

constexpr int a = g(10);    // 编译期: a = 20
int x = 10;
int b = g(x);               // 运行时: b = 30
```

| 特性 | `if constexpr` | `if consteval` |
|------|---------------|----------------|
| 决定时机 | 模板实例化时 | 每次调用时 |
| 判断依据 | 类型/编译期常量 | 当前上下文是否常量求值 |
| 用途 | 条件编译 | constexpr 函数优化 |

## 替代 std::is_constant_evaluated()

```cpp
// C++20
constexpr int abs_old(int n) {
    if (std::is_constant_evaluated()) return n < 0 ? -n : n;
    else return std::abs(n);
}

// C++23 — 更清晰，无需 <type_traits>
constexpr int abs_new(int n) {
    if consteval { return n < 0 ? -n : n; }
    else { return std::abs(n); }
}
```

## constexpr 函数优化

最实用的场景：编译期用安全但慢的实现，运行时用快速实现：

```cpp
constexpr void constexpr_sort(int* first, int* last) {
    for (auto it = first; it != last; ++it)
        for (auto jt = it + 1; jt != last; ++jt)
            if (*jt < *it) std::swap(*it, *jt);
}

constexpr void smart_sort(int* first, int* last) {
    if consteval {
        constexpr_sort(first, last);  // 编译期：慢但 constexpr 安全
    } else {
        std::sort(first, last);       // 运行时：快但非 constexpr
    }
}
```

### 哈希计算

```cpp
constexpr uint32_t hash(std::string_view s) {
    if consteval {
        uint32_t h = 2166136261u;
        for (char c : s) { h ^= static_cast<uint32_t>(c); h *= 16777619u; }
        return h;
    } else {
        return runtime_hash(s.data(), s.size());  // 可用 SIMD 等优化
    }
}
```

## 不带 else 的 if consteval

```cpp
constexpr int process(int n) {
    if consteval {
        if (n < 0) throw "negative not allowed at compile time";
    }
    return n * 2;  // 编译期和运行时都执行
}
```

## 注意事项

- `if consteval` 分支中不能调用非 constexpr 函数，即使运行时不执行
- 无 `else` 时后续代码在两种上下文都执行
- 在非 constexpr 函数中使用合法，但 `consteval` 分支在运行时永不执行
- 与 `if constexpr` 不同：前者判断"当前是否编译期上下文"，后者判断"编译期常量是否为真"
- `std::is_constant_evaluated()` 在 C++23 中仍可用，但 `if consteval` 是更推荐的写法
