---
title: C++17 std::conjunction / std::disjunction / std::negation
topic: unknown
feature: type-traits-logic
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 逻辑类型特征组合

## 概述

`std::conjunction`、`std::disjunction` 和 `std::negation` 是 C++17 在 `<type_traits>` 中引入的模板，用于**逻辑组合类型特征**。它们分别是逻辑与（`&&`）、逻辑或（`||`）和逻辑非（`!`）的编译期版本，支持**短路求值**——当结果已确定时跳过后续特征的实例化，避免编译错误和不必要的模板展开。

## 基本定义

```cpp
#include <type_traits>

// conjunction：所有特征都为 true 时结果为 true（短路与）
template <class... Bs>
struct conjunction : std::true_type {};

template <class B>
struct conjunction<B> : B {};

template <class B, class... Bs>
struct conjunction<B, Bs...>
    : std::conditional_t<bool(B::value), conjunction<Bs...>, B> {};

// disjunction：任一特征为 true 时结果为 true（短路或）
template <class... Bs>
struct disjunction : std::false_type {};

template <class B>
struct disjunction<B> : B {};

template <class B, class... Bs>
struct disjunction<B, Bs...>
    : std::conditional_t<bool(B::value), B, disjunction<Bs...>> {};

// negation：取反
template <class B>
struct negation : std::bool_constant<!bool(B::value)> {};
```

## 基本用法

```cpp
#include <type_traits>
#include <iostream>

int main() {
    // conjunction：所有特征必须为 true
    static_assert(std::conjunction<
        std::is_integral<int>,
        std::is_signed<int>,
        std::is_convertible<int, long>
    >::value, "all must be true");

    // disjunction：任一特征为 true 即可
    static_assert(std::disjunction<
        std::is_integral<int>,
        std::is_floating_point<int>
    >::value, "at least one must be true");

    // negation：取反
    static_assert(std::negation<
        std::is_pointer<int>
    >::value, "int must not be a pointer");

    std::cout << "all assertions passed\n";
}
```

## 短路求值

这是与直接使用 `&&`/`||` 的关键区别：

```cpp
#include <type_traits>

// 短路：当 is_integral<int> 为 true 时，不再实例化 is_same<int, int*>
// is_same<int, int*> 中的 int* 是不完整类型也不会导致问题
static_assert(std::conjunction<
    std::is_integral<int>,             // true → 继续
    std::is_same<int, int>             // true → 结果 true
>::value);

// 与 && 的对比：两者都会实例化所有类型
// 但 conjunction 在第一个 false 时就停止
static_assert(std::is_integral<int>::value &&
              std::is_same<int, int>::value);  // 两者都被实例化
```

短路的实际好处——避免 SFINAE 中的错误：

```cpp
#include <type_traits>
#include <iostream>

// 问题：如果 T 不是类类型，&T::value 会导致编译错误
// 使用 conjunction 可以短路，避免不必要的实例化

// 不安全的写法（短路不保证）
// template <typename T>
// using safe_check = std::bool_constant<T::value && std::is_class<T>::value>;
// 如果 T 不是类类型，T::value 会先被实例化，导致错误

// 安全的写法（conjunction 短路）
template <typename T>
using safe_check = std::conjunction<
    std::is_class<T>,           // 先检查是否是类
    std::bool_constant<T::value> // 只有是类时才检查 T::value
>;

struct Good { static constexpr bool value = true; };
struct Bad { };

int main() {
    static_assert(safe_check<Good>::value);   // OK
    static_assert(!safe_check<int>::value);   // OK，短路跳过 T::value
    static_assert(!safe_check<Bad>::value);   // OK

    std::cout << "all safe checks passed\n";
}
```

## 辅助变量模板

C++17 提供了便捷的别名模板：

```cpp
#include <type_traits>
#include <iostream>

int main() {
    // conjunction_v：直接获取 bool 值
    static_assert(std::conjunction_v<
        std::is_integral<int>,
        std::is_signed<int>
    >);

    // disjunction_v
    static_assert(std::disjunction_v<
        std::is_integral<double>,
        std::is_floating_point<double>
    >);

    // negation_v
    static_assert(std::negation_v<
        std::is_pointer<double>
    >);

    std::cout << "alias templates work\n";
}
```

## 实际应用场景

### SFINAE 约束

```cpp
#include <type_traits>
#include <iostream>
#include <string>

// 只接受整数或浮点类型
template <typename T>
std::enable_if_t<std::conjunction_v<
    std::is_arithmetic<T>,
    std::negation<std::is_same<T, bool>>
>, T>
safe_multiply(T a, T b) {
    return a * b;
}

// 只接受可移动但不可拷贝的类型
template <typename T>
std::enable_if_t<std::conjunction_v<
    std::is_move_constructible<T>,
    std::negation<std::is_copy_constructible<T>>
>, void>
process(T&& val) {
    T moved = std::move(val);
}

int main() {
    std::cout << safe_multiply(3, 4) << "\n";      // 12
    std::cout << safe_multiply(2.5, 4.0) << "\n";  // 10
    // safe_multiply(true, false);  // 编译错误：bool 被排除
}
```

### 模板特化

```cpp
#include <type_traits>
#include <iostream>
#include <vector>
#include <list>

// 为随机访问迭代器容器提供优化的 at()
template <typename Container>
typename std::enable_if_t<
    std::conjunction_v<
        std::is_same<typename Container::iterator_category,
                     std::random_access_iterator_tag>,
        std::negation<std::is_const<Container>>
    >,
    typename Container::reference
> unsafe_at(Container& c, size_t i) {
    // 随机访问：O(1)
    return c[i];
}

template <typename Container>
typename std::enable_if_t<
    std::negation_v<
        std::is_same<typename Container::iterator_category,
                     std::random_access_iterator_tag>
    >,
    typename Container::reference
> unsafe_at(Container& c, size_t i) {
    // 非随机访问：O(n)
    auto it = c.begin();
    std::advance(it, i);
    return *it;
}

int main() {
    std::vector<int> v = {10, 20, 30};
    std::cout << unsafe_at(v, 1) << "\n";  // 20

    std::list<int> l = {100, 200, 300};
    std::cout << unsafe_at(l, 2) << "\n";  // 300
}
```

### 概念模拟（C++20 之前）

```cpp
#include <type_traits>
#include <iostream>
#include <string>
#include <sstream>

// 模拟 C++20 concept：Printable
template <typename T>
using is_printable = std::conjunction<
    std::is_object<T>,
    std::negation<std::is_pointer<T>>,
    std::negation<std::is_array<T>>
>;

template <typename T>
std::enable_if_t<is_printable<T>::value>
smart_print(const T& val) {
    std::cout << val << "\n";
}

// 为指针提供特化
template <typename T>
std::enable_if_t<std::is_pointer_v<T>>
smart_print(T ptr) {
    if (ptr) {
        std::cout << *ptr << "\n";
    } else {
        std::cout << "(null)\n";
    }
}

int main() {
    smart_print(42);           // 42
    smart_print("hello");      // hello
    int x = 100;
    smart_print(&x);           // 100
}
```

## conjunction 与 && 的对比

| 特性 | `std::conjunction` | `&&` |
|------|-------------------|------|
| 求值时机 | 编译期 | 编译期 |
| 短路求值 | 支持（跳过后续实例化） | 不保证（可能全部实例化） |
| 错误处理 | 短路避免 SFINAE 错误 | 可能触发意外的编译错误 |
| 可读性 | 模板元编程中更清晰 | 简单场景更直观 |
| 返回类型 | `std::bool_constant` | `bool` |

## 编译器支持

| 编译器 | 最低版本 | 备注 |
|--------|---------|------|
| GCC | 5.0 | 完整支持 |
| Clang | 3.5 | 完整支持 |
| MSVC | 19.0 (VS 2015) | 完整支持 |

**注意**：`conjunction`/`disjunction`/`negation` 从 C++11 起就已在许多编译器中作为扩展提供，C++17 标准化后正式纳入。所有现代编译器均完整支持。

## 最佳实践

- **SFINAE 约束中优先使用**：替代 `&&`/`||`/`!` 可以避免短路不一致导致的编译错误。
- **使用 `_v` 后缀**：`conjunction_v<Bs...>` 比 `conjunction<Bs...>::value` 更简洁。
- **组合多个类型特征**：创建复杂的类型约束，为模板特化提供精确控制。
- **在 Concepts 之前提供约束**：C++20 之前，这是实现概念模拟的主要工具。

## 常见陷阱

```cpp
// 陷阱 1：conjunction 短路但 && 不短路
// conjunction 在第一个 false 时停止实例化
// && 可能实例化所有操作数（取决于编译器优化）

// 陷阱 2：类型特征的 value 成员
// conjunction 的模板参数必须是继承自 true_type/false_type 的类型
// 不能直接使用 bool
// std::conjunction<std::true_type, true>  // 编译错误！

// 陷阱 3：空参数列表
static_assert(std::conjunction<>::value);   // true（默认继承 true_type）
static_assert(!std::disjunction<>::value);  // false（默认继承 false_type）

// 陷阱 4：与 fold expressions 的对比
// C++17 fold expressions 也可以实现逻辑组合，但不提供短路
// template <typename... Bs>
// using conjunction_fold = std::bool_constant<(Bs::value && ...)>;
// 这不保证短路！
```
