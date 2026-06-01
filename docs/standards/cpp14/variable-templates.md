# C++14 变量模板 (Variable Templates)

## 概述

C++14 引入变量模板，允许定义依赖于模板参数的编译期常量变量。在 C++11 中，要实现类似效果需要借助静态成员函数（`value`）或 `constexpr` 函数，代码冗长。变量模板使得类型参数化的常量定义变得直观。

## 语法

```cpp
template <typename T>
constexpr T variable_name = initial_value;

// 使用
auto val = variable_name<double>;
```

## 代码示例

### 基本用法：数学常量

```cpp
#include <cmath>
#include <iostream>

// 不同精度的 pi 常量
template <typename T>
constexpr T pi = T(3.141592653589793238462643383279502884L);

int main() {
    std::cout << pi<float>  << '\n';   // 3.14159 (float 精度)
    std::cout << pi<double> << '\n';   // 3.141592653589793 (double 精度)
}
```

### 与类型萃取配合

```cpp
#include <type_traits>

// C++11 方式：需要 ::value
template <typename T>
void check() {
    static_assert(std::is_integral<T>::value, "must be integral");
}

// C++14 标准库已提供 _v 后缀的变量模板
// 等价于 std::is_integral<T>::value
template <typename T>
void check_v2() {
    static_assert(std::is_integral_v<T>, "must be integral");
}

// 自定义类型萃取的变量模板
template <typename T>
constexpr bool is_small = sizeof(T) <= sizeof(int);

static_assert(is_small<char>);
static_assert(!is_small<double>);
```

### 带约束的变量模板

```cpp
#include <type_traits>

// 仅对算术类型有效
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
constexpr T zero = T(0);

// 特化：对浮点类型返回极小值而非零
template <typename T>
constexpr T epsilon = T(1e-10);

template <>
constexpr float epsilon<float> = 1e-6f;

template <>
constexpr double epsilon<double> = 1e-10;
```

### 类中的变量模板（C++14 静态数据成员模板）

```cpp
#include <cstddef>

struct Config {
    // 类内变量模板 — C++14 起隐式 inline（非模板类中需 C++17 inline）
    template <typename T>
    static constexpr std::size_t max_size = 1024;
};

// 类外特化
template <>
constexpr std::size_t Config::max_size<char> = 4096;

template <>
constexpr std::size_t Config::max_size<double> = 128;
```

### 编译期计算场景

```cpp
#include <cstddef>

// 编译期阶乘表
template <std::size_t N>
constexpr std::size_t factorial = N * factorial<N - 1>;

template <>
constexpr std::size_t factorial<0> = 1;

// 用于数组大小
int table[factorial<5>];  // 120 个元素的数组

// 编译期单位转换
template <typename T>
constexpr T inches_to_cm = T(2.54);

template <typename T>
constexpr T miles_to_km = T(1.60934);
```

### 变量模板 vs constexpr 函数

```cpp
// 方式 A：constexpr 函数
constexpr double pi_func() { return 3.141592653589793; }

// 方式 B：变量模板
template <typename T = double>
constexpr T pi_var = T(3.141592653589793238462643383279502884L);

// 变量模板的优势：
// 1. 语法更简洁：pi<double> vs pi_func()  — 但函数风格也常见
// 2. 可特化：pi<long double> 可以提供更高精度
// 3. 可作为模板参数传递

template <typename T, T Value>
struct Constant {};

// 变量模板可以参与类型构造
Constant<double, pi_var<double>> c;  // OK
// Constant<double, pi_func()> c2;   // C++20 起才允许非类型模板参数为浮点
```

## 最佳实践

1. **优先使用变量模板替代 `::value` 后缀**：自定义类型萃取时，同时提供 `::value` 和 `_v` 变量模板，与标准库保持一致。
2. **为数学常量使用变量模板**：`pi<T>` 比多个 `#define` 或 `constexpr auto pi_f = ...; constexpr auto pi_d = ...;` 更优雅。
3. **注意模板实例化开销**：变量模板在每次使用处可能实例化，但链接器会合并相同实例，不会产生重复存储。
4. **变量模板不能被 `constexpr` 函数约束（C++14/17）**：需要 SFINAE 约束时，通过默认模板参数 + `std::enable_if` 实现。
5. **C++17 起类内静态变量模板隐式 inline**：在 C++14 中，类内静态变量模板的定义放在头文件中可能出现多重定义，需要注意 ODR。
6. **避免与同名函数/类型冲突**：变量模板引入了新的命名空间层级的变量名，应确保不与宏、函数或类型名冲突。
