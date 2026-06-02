---
title: "C++11 std::tuple 元组"
topic: unknown
feature: tuple
standard: N/A
status_checked_at: 2026-06-02
---
# C++11 std::tuple 元组

## 概述

`std::tuple` 是 C++11 引入的固定大小异构容器，定义在 `<tuple>` 中。它将任意数量、任意类型的值聚合为一个对象，类似于结构体但无需命名字段。tuple 在泛型编程中尤为重要——标准库多处接口（`std::thread`、`std::promise`、无序容器返回值）都依赖它打包多个值。

相比 `std::pair`（仅两个元素），tuple 支持任意数量。相比手写结构体，tuple 无需额外类型声明，适合临时聚合和泛型上下文。

## 核心 API

### 创建 tuple

```cpp
#include <tuple>
#include <string>

// 直接构造
std::tuple<int, std::string, double> t1(42, "hello", 3.14);

// make_tuple — 自动推导类型（会退化引用和 cv 限定）
auto t2 = std::make_tuple(42, "hello", 3.14);
// 类型为 tuple<int, const char*, double>

// C++14 起支持按类型获取（类型必须唯一）
std::cout << std::get<std::string>(t1) << "\n";
```

### `std::get` — 访问元素

```cpp
auto t = std::make_tuple(1, std::string("hello"), 3.14);

int n = std::get<0>(t);           // 1（按索引，编译期常量）
std::string s = std::get<1>(t);   // "hello"
double d = std::get<double>(t);   // 3.14（C++14，按类型）

// get 返回引用，可用于修改
std::get<0>(t) = 100;

// 移动语义版本
auto moved = std::get<1>(std::move(t));
// t 中的 string 处于有效但未指定状态
```

### `tie` — 解包 tuple

```cpp
#include <tuple>

std::tuple<int, std::string, double> getData() {
    return {1, "hello", 3.14};
}

// C++11/14 的解包方式
int x;
std::string y;
double z;
std::tie(x, y, z) = getData();

// std::ignore 忽略不需要的元素
int id;
std::tie(id, std::ignore, std::ignore) = getData();

// tie 用于字典序比较——非常实用
auto t1 = std::make_tuple(1, "alpha");
auto t2 = std::make_tuple(1, "beta");
if (t1 < t2) {  // 先比第一个元素，相等则比第二个
    std::cout << "t1 < t2\n";
}
```

### `forward_as_tuple` — 转发元组

```cpp
#include <tuple>

// forward_as_tuple 保持参数的引用类别（左值/右值）
// 主要用于完美转发场景

template<typename... Args>
void wrapper(Args&&... args) {
    auto t = std::forward_as_tuple(std::forward<Args>(args)...);
    // t 中元素保持原值类别：左值引用或右值引用
}

// 实用场景：emplace 系列函数的实现基础
std::vector<std::pair<int, std::string>> vec;
int key = 42;
std::string value = "hello";
// forward_as_tuple 使 value 作为左值引用被转发，避免拷贝
```

### `tuple_cat` — 拼接元组

```cpp
#include <tuple>

auto t1 = std::make_tuple(1, 2);
auto t2 = std::make_tuple(3.0, "hello");
auto t3 = std::make_tuple(std::string("world"));

// 拼接多个 tuple
auto combined = std::tuple_cat(t1, t2, t3);
// tuple<int, int, double, const char*, std::string>

static_assert(std::tuple_size<decltype(combined)>::value == 5, "");

// 实用：在 tuple 前面追加元素
auto with_prefix = std::tuple_cat(std::make_tuple(0), t1);
// (0, 1, 2)
```

### `tuple_size` 和 `tuple_element` — 类型查询

```cpp
#include <tuple>
#include <type_traits>

using MyTuple = std::tuple<int, std::string, double>;

static_assert(std::tuple_size<MyTuple>::value == 3, "");

// 获取第 N 个元素的类型（编译期）
using E0 = std::tuple_element<0, MyTuple>::type;  // int
using E1 = std::tuple_element<1, MyTuple>::type;  // std::string

// 对 const tuple 保留 const 限定
using CE1 = std::tuple_element<1, const MyTuple>::type;  // const std::string

static_assert(std::is_same<E0, int>::value, "");
static_assert(std::is_same<CE1, const std::string>::value, "");
```

### 比较运算符

tuple 比较是**逐元素字典序**，所有六种运算符（`==`、`!=`、`<`、`>`、`<=`、`>=`）均可用：

```cpp
auto a = std::make_tuple(1, 2, 3);
auto b = std::make_tuple(1, 2, 4);
std::cout << (a < b) << "\n";  // 1（第三个元素 3 < 4）

// 天然适合作为复合排序键
std::vector<std::tuple<int, std::string>> students = {
    {90, "Alice"}, {85, "Bob"}, {90, "Charlie"}, {85, "Aaron"}
};
std::sort(students.begin(), students.end(),
    [](const auto& a, const auto& b) {
        return std::make_tuple(-std::get<0>(a), std::get<1>(a))
             < std::make_tuple(-std::get<0>(b), std::get<1>(b));
    });
// 结果：(90, Alice), (90, Charlie), (85, Aaron), (85, Bob)
```

## 作为函数返回类型

tuple 最常见的用途是从函数返回多个值：

```cpp
#include <tuple>
#include <string>

std::tuple<bool, double, std::string> divide(double a, double b) {
    if (b == 0.0) return {false, 0.0, "division by zero"};
    return {true, a / b, "ok"};
}

int main() {
    // C++11/14
    bool ok; double val; std::string error;
    std::tie(ok, val, error) = divide(10.0, 0.0);
    // ok=false, val=0.0, error="division by zero"
}
```

## C++17 结构化绑定（提及）

C++17 大幅简化了 tuple 的使用：

```cpp
// C++17 —— 直接解包
auto [id, name, value] = std::make_tuple(42, "hello", 3.14);

// 用于 for 循环
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}
```

## 实用模式

### 用 index_sequence 展开 tuple

```cpp
#include <tuple>
#include <utility>
#include <iostream>

template<typename Tuple, typename Func, std::size_t... Is>
void for_each_impl(Tuple&& t, Func&& f, std::index_sequence<Is...>) {
    using swallow = int[];
    (void)swallow{0, (void(f(std::get<Is>(std::forward<Tuple>(t)))), 0)...};
}

template<typename Tuple, typename Func>
void for_each(Tuple&& t, Func&& f) {
    constexpr std::size_t N =
        std::tuple_size<typename std::decay<Tuple>::type>::value;
    for_each_impl(std::forward<Tuple>(t), std::forward<Func>(f),
                  std::make_index_sequence<N>{});
}

auto t = std::make_tuple(1, "hello", 3.14);
for_each(t, [](const auto& elem) {
    std::cout << elem << "\n";  // 1, hello, 3.14
});
```

### 用 `tie` 实现 `operator<`

```cpp
struct Record {
    std::string name;
    int age;
    int id;

    bool operator<(const Record& rhs) const {
        return std::tie(name, age, id)
             < std::tie(rhs.name, rhs.age, rhs.id);
    }
};
```

## 最佳实践

1. **`auto` + `make_tuple` 简化声明**：避免冗长的类型签名。
2. **`tie` + `ignore` 选择性解包**：只关心部分返回值时使用 `std::ignore`。
3. **用 `tie` 实现 `operator<`**：避免手写嵌套比较的样板代码。
4. **`forward_as_tuple` 仅用于转发上下文**：它持有引用，不要将生命周期延长到被引用对象之外。
5. **C++17 项目优先使用结构化绑定**：可读性远优于 `get<0>` 调用，无性能损失。

## 常见陷阱

- **`make_tuple` 退化类型**：移除引用和 cv 限定。`std::string s; auto t = make_tuple(s);` 持有拷贝。需 `std::ref(s)` 或 `std::tie(s)` 保持引用。
- **`get` 索引越界是编译错误**：`N` 必须小于 tuple 大小。编译期检查，但错误信息可能难以阅读。
- **`get<Type>` 类型必须唯一**（C++14）：`tuple<int, int>` 上调用 `std::get<int>` 编译失败。
- **移动语义与 get**：`std::get<0>(std::move(t))` 移动后该元素处于有效但未指定状态。
- **`forward_as_tuple` 悬垂引用**：
  ```cpp
  auto dangling() {
      std::string s = "temp";
      return std::forward_as_tuple(s);  // 危险！返回局部变量的引用
  }
  ```
  仅搭配完美转发使用，不用于返回值。
- **编译时间和错误信息**：嵌套 tuple 类型会导致错误信息极长，考虑为 tuple 类型定义别名。
