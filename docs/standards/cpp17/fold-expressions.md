# C++17 折叠表达式（Fold Expressions）

## 概述

折叠表达式是 C++17 引入的语言特性，允许对参数包中的所有参数施加一个二元运算符，将其"折叠"为单个值。消除了以往需要递归模板或初始化列表展开处理变参模板的繁琐写法，是 C++17 中最实用的语法糖之一。

## 四种折叠形式

```cpp
// 一元右折叠
(args op ...)          →  (E₁ op (E₂ op (... op Eₙ)))

// 一元左折叠
(... op args)          →  (((E₁ op E₂) op ...) op Eₙ)

// 二元右折叠
(args op ... op init)  →  (E₁ op (E₂ op (... op (Eₙ op init))))

// 二元左折叠
(init op ... op args)  →  ((((init op E₁) op E₂) op ...) op Eₙ)
```

支持的运算符包括算术、位运算、比较、`&&`、`||`、`,` 等。

## 求和示例

```cpp
#include <iostream>

// 一元折叠：空包时编译错误
template<typename... Args>
auto sum(Args... args) { return (args + ...); }

// 二元折叠：带初始值，空包安全
template<typename... Args>
auto sum_safe(Args... args) { return (0 + ... + args); }

int main() {
    std::cout << sum(1, 2, 3, 4, 5) << "\n";      // 15
    std::cout << sum_safe() << "\n";               // 0
}
```

一元折叠对空包无默认值（`&&`→`true`、`||`→`false`、`,`→`void()` 除外），二元折叠始终安全。

## 逻辑折叠

```cpp
#include <type_traits>

template<typename... Ts>
constexpr bool all_integral() {
    return (std::is_integral_v<Ts> && ...);
}

template<typename... Args>
bool all_positive(Args... args) {
    return ((args > 0) && ...);
}

static_assert(all_integral<int, long, char>());
static_assert(!all_integral<int, double>());
```

`&&` 和 `||` 折叠具有短路求值语义。

## 逗号折叠：遍历参数包

```cpp
#include <iostream>

template<typename... Args>
void print_all(Args... args) {
    ((std::cout << args << " "), ...);  // 逗号折叠
    std::cout << "\n";
}

print_all(1, "hello", 3.14, 'X');  // 1 hello 3.14 X
```

## Lambda 折叠

```cpp
#include <string>

template<typename... Args>
std::string join(const std::string& sep, Args... args) {
    std::string result;
    bool first = true;
    auto append = [&](const auto& val) {
        if (!first) result += sep;
        result += std::to_string(val);
        first = false;
    };
    (append(args), ...);
    return result;
}

template<typename Func, typename... Args>
void for_each(Func func, Args&&... args) {
    (func(std::forward<Args>(args)), ...);
}
```

## 左折叠 vs 右折叠

对非结合运算符结果不同：

```cpp
// 左折叠：(((1 - 2) - 3) - 4) = -8
(... - args)

// 右折叠：(1 - (2 - (3 - 4))) = -2
(args - ...)
```

对结合运算符（`+`、`*`、`&&`、`||`），两种方向结果相同。

## 实际用例

```cpp
// 编译期类型检查
template<typename T, typename... Ts>
constexpr bool all_same() {
    return (std::is_same_v<T, Ts> && ...);
}

// 容器批量操作
template<typename Container, typename... Values>
void push_all(Container& c, Values&&... values) {
    (c.push_back(std::forward<Values>(values)), ...);
}

// 多条件匹配
template<typename T, typename... Args>
bool is_any_of(const T& val, const Args&... args) {
    return ((val == args) || ...);
}
```

## 空包处理规则

| 折叠形式 | 空包结果 |
|----------|----------|
| `(args && ...)` | `true` |
| `(args \|\| ...)` | `false` |
| `(args, ...)` | `void()` |
| 其他一元折叠 | **编译错误** |
| 二元折叠 `(init op ... op args)` | 返回 `init` |

## 最佳实践

- **优先使用二元折叠**以安全处理空包。
- **短路逻辑**：`&&`/`||` 折叠可提前终止。
- **副作用操作**：逗号折叠 `(func(args), ...)` 遍历参数包。
- **完美转发折叠**：`(std::forward<Args>(args), ...)` 是变参转发标准模式。
- **确保类型一致**：运算符必须对所有参数类型合法。

## 常见陷阱

```cpp
// 陷阱 1：空包一元折叠
template<typename... Args>
auto bad_sum(Args... args) { return (args + ...); }
// bad_sum() 编译错误——修复：(0 + ... + args)

// 陷阱 2：折叠方向影响非结合运算
// (args - ...) ≠ (... - args)

// 陷阱 3：运算符优先级
// 错误：return ((args == 42) && ...);  // 需要括号保证优先级
```
