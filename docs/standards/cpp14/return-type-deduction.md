# C++14 返回类型推导 (Return Type Deduction)

## 概述

C++14 允许函数使用 `auto` 作为返回类型，编译器从 `return` 语句推导实际类型。这消除了冗长的尾置返回类型语法，简化了模板函数的编写。对于多条 `return` 语句，所有语句必须推导出相同的类型。

## 语法

```cpp
// 基本形式
auto func(params) {
    return expr;  // 返回类型从 expr 推导
}

// 等价的 C++11 尾置返回类型写法
auto func(params) -> decltype(expr) {
    return expr;
}
```

## 核心规则

1. **所有 return 语句必须推导出同一类型**（忽略 cv 限定差异时）。
2. **递归函数**必须至少有一个 return 语句在递归调用之前，以便编译器先推导出类型。
3. **虚函数不能使用 `auto` 返回类型**。
4. **函数声明与定义必须一致**——若头文件中声明了 `auto foo();`，定义中必须可见 return 语句才能推导。

## 代码示例

### 基本用法

```cpp
// 简单推导
auto add(int a, int b) {
    return a + b;  // 推导为 int
}

auto compute(double x) {
    return x * 2.5;  // 推导为 double
}
```

### 多条 return 语句

```cpp
#include <string>

// 两条 return 都是 std::string — OK
auto make_greeting(bool formal) {
    if (formal) {
        return std::string("Good morning, sir.");
    }
    return std::string("Hey!");
}

// 错误示例：类型不一致
auto bad(bool flag) {
    if (flag) return 42;      // int
    return 3.14;              // double — 编译错误！
}
```

### 模板函数中的推导

```cpp
#include <vector>
#include <type_traits>

// 推导为容器的 value_type 引用
template <typename Container>
auto get_first(Container& c) -> decltype(c.front()) {
    return c.front();
}

// C++14 可以直接写：
template <typename Container>
auto get_first_v2(Container& c) {
    return c.front();  // 推导 c.front() 的返回类型
}
```

### 递归函数的限制

```cpp
// 错误：编译器无法推导返回类型
// auto factorial(int n) {
//     if (n <= 1) return 1;
//     return n * factorial(n - 1);  // factorial 尚未推导完成
// }

// 正确方案 1：显式指定返回类型
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// 正确方案 2：尾置返回类型 + auto
auto factorial_v2(int n) -> int {
    if (n <= 1) return 1;
    return n * factorial_v2(n - 1);
}

// 正确方案 3：利用 non-recursive base case 在前
// 这种写法可行，但不推荐 — 可读性差
auto factorial_v3(int n) {
    if (n <= 1) return 1;  // 此处推导为 int
    return n * factorial_v3(n - 1);  // 这条也在推导之后，OK
}
```

### 返回引用 vs 返回值

```cpp
#include <vector>

std::vector<int> data = {10, 20, 30};

// auto 会推导出值类型（去掉引用和顶层 const）
auto get_value(int index) {
    return data[index];  // 推导为 int（不是 int&）
}

// 使用 decltype(auto) 保留引用
decltype(auto) get_ref(int index) {
    return data[index];  // 推导为 int&
}
```

### 与 `decltype(auto)` 的对比

```cpp
template <typename Container, typename Index>
auto access_v1(Container& c, Index i) {
    return c[i];       // 丢弃引用 — 推导为值类型
}

template <typename Container, typename Index>
decltype(auto) access_v2(Container& c, Index i) {
    return c[i];       // 保留引用 — 如果 c[i] 返回 T&，这里也是 T&
}
```

## 最佳实践

1. **简单函数优先 `auto` 返回类型**：对于只有一条 return 语句的短函数，`auto` 最简洁。
2. **需要保留引用时用 `decltype(auto)`**：当返回的是左值引用（如容器下标访问），`auto` 会丢失引用语义，必须使用 `decltype(auto)`。
3. **递归函数显式指定返回类型**：虽然有些编译器接受 base-case-in-front 的写法，但显式类型更安全、更易读。
4. **注意 SFINAE 交互**：`auto` 返回类型不参与 SFINAE；需要约束返回类型时，使用尾置 `-> decltype(expr)` 配合 `std::enable_if`。
5. **头文件中的声明**：如果函数声明和定义分离，`auto` 返回类型要求定义对调用者可见（通常放在头文件中），否则编译器无法推导。
6. **避免推导出非预期类型**：使用 `static_assert(std::is_same_v<decltype(f()), Expected>)` 来锁定返回类型，防止重构时意外变化。
