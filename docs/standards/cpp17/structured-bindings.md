# C++17 结构化绑定（Structured Bindings）

## 概述

结构化绑定是 C++17 引入的语法特性，允许将复合对象（`pair`、`tuple`、数组、聚合体结构体）的成员一次性解构并绑定到多个变量上。形式为 `auto [a, b] = expr;`，消除了以往需要 `std::tie` 或手动逐个访问成员的繁琐写法。

## 基本语法

```cpp
auto [id_1, id_2, ..., id_n] = expression;    // 值绑定
auto& [id_1, id_2, ..., id_n] = expression;   // 引用绑定
const auto& [id_1, id_2, ..., id_n] = expr;   // const 引用绑定
```

绑定变量的数量必须与对象中可访问的成员数量一致。

## 绑定到 pair 和 tuple

```cpp
#include <utility>
#include <tuple>
#include <iostream>

int main() {
    std::pair<int, std::string> p{42, "hello"};
    auto [val, str] = p;
    std::cout << val << ", " << str << "\n"; // 42, hello

    std::tuple<double, int, char> t{3.14, 7, 'X'};
    auto [d, i, c] = t;

    // 引用绑定——修改反映到原对象
    auto& [dref, iref, cref] = t;
    iref = 99;
    std::cout << std::get<1>(t) << "\n"; // 99
}
```

## 绑定到数组

```cpp
int arr[] = {10, 20, 30};
auto [x, y, z] = arr;       // x=10, y=20, z=30

auto& [rx, ry, rz] = arr;   // 引用绑定
rx = 100;                    // arr[0] 现在是 100

// std::array 同样适用
std::array<int, 3> a = {1, 2, 3};
auto [a0, a1, a2] = a;
```

## 绑定到聚合体结构体

```cpp
struct Point {
    double x;
    double y;
    double z;
};

Point pt{1.0, 2.0, 3.0};
auto [px, py, pz] = pt;  // 按成员顺序绑定
```

要求类型是**完整类型**（complete type），绑定数量必须恰好等于非静态数据成员数量，不能只绑定部分成员。

## map 迭代中的使用

```cpp
#include <map>
#include <string>

std::map<std::string, int> scores = {
    {"Alice", 95}, {"Bob", 87}, {"Carol", 92}
};

// 比 it->first / it->second 清晰得多
for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}
```

这是结构化绑定最常见的用法之一。

## 引用绑定与值绑定

```cpp
std::tuple<int, std::string> t{3, "edit"};

// 值绑定：独立副本
auto [val, str] = t;

// const 引用绑定：可延长临时对象生命周期
const auto& [cval, cstr] = std::tuple<int, std::string>{2, "temp"};

// 非 const 引用绑定：就地修改
auto& [mval, mstr] = t;
mstr = "edited";  // t 中的 string 被修改
```

注意：`auto&` 不能绑定到临时对象，`const auto&` 和 `auto&&` 可以延长生命周期。

## 底层机制

结构化绑定是编译器的语法糖。对聚合体类型，编译器生成隐藏变量 `e`，每个绑定变量引用 `e` 的对应成员：

```cpp
// auto [a, b] = point;
// 等价于（概念上）：
auto __e = point;
auto& a = __e.x;
auto& b = __e.y;
```

对于 `tuple`-like 类型使用 `std::get<I>(__e)`；对于数组使用 `__e[I]`。

## 最佳实践

- **优先用于 `map`/`unordered_map` 迭代**，大幅提升可读性。
- **函数返回多个值时**，用 `tuple` 或结构体配合结构化绑定，比输出参数更清晰。
- **需要修改原对象时**，使用 `auto&` 或 `const auto&` 绑定。
- **避免对大对象做值绑定**，应使用引用绑定避免拷贝。
- **需要忽略某些成员时**，回退到 `std::tie` 配合 `std::ignore`。

## 常见陷阱

```cpp
// 陷阱 1：位域成员不能使用结构化绑定
struct Flags { unsigned int read : 1; unsigned int write : 1; };
Flags f{1, 0};
// auto [r, w] = f;  // 错误：位域不能被引用

// 陷阱 2：数量不匹配
// auto [a, b] = std::tuple<int,int,int>{1,2,3};  // 错误：3个成员但只绑2个

// 陷阱 3：值绑定不修改原对象
std::pair<int,int> p{1, 2};
auto [a, b] = p;
a = 99;
// p.first 仍然是 1——a 是独立副本

// 陷阱 4：vector 不支持结构化绑定
// auto [a, b, c] = std::vector{1, 2, 3};  // 错误：非聚合体/tuple-like
```
