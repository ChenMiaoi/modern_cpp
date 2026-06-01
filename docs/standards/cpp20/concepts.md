# C++20 Concepts（概念）

## 概述

C++20 引入了 Concepts，这是对模板系统最重大的改进之一。Concepts 允许开发者为模板参数定义具名约束，将类型要求从注释提升为编译器可检查的一等公民。在此之前，模板错误信息晦涩难懂，约束只能通过 SFINAE 技巧间接表达。Concepts 解决了这些问题：错误信息直接指出哪个约束不满足，代码意图一目了然，且不再需要 `enable_if` 等元编程样板代码。

C++20 标准库在 `<concepts>` 和 `<ranges>` 头文件中提供了大量预定义概念。

## 定义 Concept

语法形式为 `template <参数列表> concept 名称 = 约束表达式;`，约束表达式必须求值为 `bool` 常量表达式。

```cpp
#include <concepts>
#include <type_traits>

// 基于类型特征定义
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

// 组合已有 concept
template <typename T>
concept SignedInteger = std::integral<T> && std::is_signed_v<T>;

// 多参数 concept
template <typename From, typename To>
concept ImplicitlyConvertibleTo = requires(From(&f)()) {
    { f() } -> std::convertible_to<To>;
};
```

## requires 表达式与 requires 子句

`requires` 有两种角色：**requires 表达式**是一个产生 `bool` 的表达式，检查操作是否合法；**requires 子句**放在模板参数列表之后，约束模板。

```cpp
#include <concepts>
#include <iostream>

// requires 表达式——检查类型是否可输出到 ostream
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};

// requires 子句——约束函数模板
template <typename T>
    requires Printable<T>
void log(const T& value) {
    std::cout << value << '\n';
}

// 简单 requires、类型 requires、复合 requires 可组合
template <typename T>
concept Sortable = requires(T& container) {
    typename T::value_type;                              // 类型 requires
    { container.begin() } -> std::input_or_output_iterator;
    { container.end() }   -> std::input_or_output_iterator;
};
```

## 预定义 Concepts

```cpp
#include <concepts>

// same_as: T 与 U 是同一类型（含 cv/ref 限定）
static_assert(std::same_as<int, int>);
static_assert(!std::same_as<int, const int>);

// convertible_to: From 可隐式转换为 To，且 static_cast<To> 合法
static_assert(std::convertible_to<int, double>);
static_assert(!std::convertible_to<int, std::string>);

// integral: T 是整型类型（bool、char、int、long 等）
static_assert(std::integral<int>);
static_assert(std::integral<char>);
static_assert(!std::integral<double>);

// floating_point: T 是浮点类型
static_assert(std::floating_point<float>);
static_assert(std::floating_point<double>);
static_assert(!std::floating_point<int>);
```

```cpp
// derived_from<D, B>: D 公有派生自 B（含自身）
struct Base {};
struct Derived : Base {};
static_assert(std::derived_from<Derived, Base>);
static_assert(!std::derived_from<Base, Derived>);

// invocable<F, Args...>: F 可以用 Args... 调用
auto square = [](int x) { return x * x; };
static_assert(std::invocable<decltype(square), int>);
static_assert(!std::invocable<decltype(square), std::string>);
```

## 使用 Concepts 约束模板

三种方式将 concept 应用于模板参数：

```cpp
#include <concepts>
#include <vector>
#include <algorithm>

// 方式一：constrained template parameter（概念前置）
template <std::integral T>
T gcd(T a, T b) {
    while (b != 0) { T tmp = b; b = a % b; a = tmp; }
    return a;
}

// 方式二：requires 子句（适用于复杂约束）
template <typename T>
    requires std::floating_point<T>
T average(T a, T b) {
    return (a + b) / T{2};
}

// 方式三：两者结合
template <std::ranges::range R>
    requires std::sortable<std::ranges::iterator_t<R>>
void my_sort(R& range) {
    std::ranges::sort(range);
}
```

## 缩写函数模板与 Constrained auto

C++20 允许用 `Concept auto` 替代完整模板声明。每个 `auto` 生成独立模板参数。

```cpp
#include <ranges>
#include <vector>
#include <iostream>

// 缩写函数模板：std::ranges::range auto&
void print_range(std::ranges::range auto&& container) {
    for (const auto& elem : container)
        std::cout << elem << ' ';
    std::cout << '\n';
}

int main() {
    // constrained auto 作为变量声明
    std::integral auto x = 42;
    std::floating_point auto pi = 3.14159;

    std::vector<int> v{5, 2, 8, 1, 9};
    print_range(v);   // OK

    // 错误示例（编译期失败）：
    // std::integral auto bad = 3.14;  // double 不满足 integral
}
```

## Concept vs SFINAE vs static_assert

| 维度 | Concept | SFINAE（`enable_if`） | `static_assert` |
|------|---------|----------------------|-----------------|
| 错误信息 | 明确指出不满足的约束 | 冗长且晦涩 | 直接但不参与重载决议 |
| 重载决议 | 参与，选择最特化匹配 | 参与，语法复杂 | 不参与——直接硬错误 |
| 编译速度 | 更快（约束缓存） | 较慢（反复实例化） | 快 |
| 可读性 | 高 | 低 | 中 |
| 组合性 | `&&`、`||` 自然组合 | 需 `conjunction`/`disjunction` | 不适用 |

```cpp
// C++17 SFINAE
template <typename T,
          typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T clamp_val(T v, T lo, T hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

// C++20 Concepts——等价但更清晰
template <std::integral T>
T clamp_val2(T v, T lo, T hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}
```

## 最佳实践

1. **优先使用标准库 concept**。`std::integral`、`std::ranges::range` 等经过充分测试，语义明确。
2. **用 concept 替代 SFINAE**。新代码中不应再出现 `std::enable_if`；已有 SFINAE 代码逐步迁移。
3. **concept 命名使用形容词短语**：`Sortable`、`Hashable`、`Printable`，反映类型应满足的语义。
4. **保持 concept 粒度细**。一个 concept 只表达一项约束，组合时用 `&&` 连接，提高复用性。
5. **在声明处约束，不在定义处重复**。头文件已写 `requires`，源文件不要重复。

## 常见陷阱

1. **`requires` 表达式与 `requires` 子句混淆**。`requires expr { ... }` 产生 `bool`；`template <...> requires C` 约束模板。两者语法不同，用途不同。
2. **constrained auto 每个 `auto` 是独立模板参数**。`f(std::integral auto a, std::integral auto b)` 中 `a` 和 `b` 类型可以不同。如需同类型，必须显式写 `template <std::integral T> void f(T a, T b);`。
3. **concept 不是类型**。不能写 `std::integral x = 42;`——concept 是编译期谓词，仅用于约束。
4. **subsumption 规则容易误判**。编译器仅在语法层面判断约束的包含关系；两个语义相同但语法不同的 concept 不会互相替代。
5. **不要用 `static_assert` 替代 concept 做重载选择**。`static_assert` 失败时不会回退到其他重载——应使用 concept 或 requires 子句实现 SFINAE 语义。