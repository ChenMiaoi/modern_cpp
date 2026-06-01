# C++17 std::variant

## 概述

`std::variant<Types...>` 是 C++17 在 `<variant>` 中引入的类型安全联合体（tagged union）。与 C `union` 不同，`std::variant` 在编译期跟踪当前存储的类型，提供类型安全的访问方式。它可持有模板参数列表中**恰好一种**类型的值，默认初始化为第一个类型的默认值。

## 创建与赋值

```cpp
#include <variant>
#include <string>

// 默认构造：持有第一个类型的默认值
std::variant<int, double, std::string> v1;  // int{0}

// 值初始化
std::variant<int, double, std::string> v2{42};
std::variant<int, double, std::string> v3{3.14};
std::variant<int, double, std::string> v4{"hello"};       // const char*
std::variant<int, double, std::string> v5{std::string{"hello"}}; // 显式 string

// 类型歧义时用 in_place_type
std::variant<int, long, double> v{std::in_place_type<long>, 42L};

// 赋值改变活跃类型
v1 = "new value";  // 转为 string
v1 = 100;          // 转回 int
```

## 访问值：std::get 与 std::get_if

```cpp
std::variant<int, double, std::string> v{3.14};

// 按类型或索引访问——类型不匹配时抛 std::bad_variant_access
double d  = std::get<double>(v);  // OK
double d2 = std::get<1>(v);       // 索引 1 对应 double

try {
    int i = std::get<int>(v);     // 当前是 double，抛异常
} catch (const std::bad_variant_access& e) {
    std::cout << e.what() << "\n";
}

// get_if：返回指针，不匹配时返回 nullptr
auto* p = std::get_if<std::string>(&v);
if (p) std::cout << *p << "\n";
```

## 类型检查：std::holds_alternative

```cpp
std::variant<int, double, std::string> v{42};

if (std::holds_alternative<int>(v)) {
    std::cout << "int: " << std::get<int>(v) << "\n";
}

// v.index() 返回活跃类型的零基索引
std::cout << v.index() << "\n";  // 0
```

## std::visit 与重载 lambda 模式

```cpp
#include <variant>
#include <iostream>

// 辅助模板：组合多个 lambda 为一个 overload 集
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;  // C++17 推导指引

void demo() {
    std::variant<int, double, std::string> v{"hello"};

    std::visit(overloaded{
        [](int i) { std::cout << "int: " << i << "\n"; },
        [](double d) { std::cout << "double: " << d << "\n"; },
        [](const std::string& s) { std::cout << "string: " << s << "\n"; }
    }, v);
}
```

`overloaded` 继承所有 lambda 的 `operator()`，是社区广泛采用的惯用法。

### 兜底处理与多 variant

```cpp
// 兜底：用 auto&& 匹配其余类型
std::visit(overloaded{
    [](int i) { /* 特化 */ },
    [](auto&& x) { /* 兜底 */ }
}, v);

// 多 variant 同时访问
std::variant<int, std::string> a{1};
std::variant<double, std::string> b{"world"};
std::visit(overloaded{
    [](int ia, const std::string& sb) { /* ... */ },
    [](const auto& x, const auto& y) { /* ... */ }
}, a, b);
```

## std::monostate：表示空状态

`std::variant` 始终持有某种值。需要"无值"时使用 `std::monostate`（零字节空结构体）：

```cpp
std::variant<std::monostate, int, std::string> v;  // 默认持有 monostate

if (std::holds_alternative<std::monostate>(v)) {
    std::cout << "empty\n";
}

v = 42;               // 持有 int
v = std::monostate{}; // 回到"空"
```

## valueless_by_exception

赋值或 `emplace` 时构造新值抛异常，variant 可能进入 `valueless_by_exception` 状态：

```cpp
std::variant<int, std::string> v{42};
// 如果 string 构造抛异常，v.valueless_by_exception() 为 true
// 此后 std::get 会抛异常，holds_alternative 全部返回 false

if (v.valueless_by_exception()) {
    std::cout << "variant is in error state\n";
}
```

确保 variant 中的类型有 `noexcept` 移动构造函数可最小化此风险。

## 与 union 和 any 的比较

| 特性 | C `union` | `std::variant` | `std::any` |
|------|-----------|-----------------|------------|
| 类型安全 | 否 | 是（编译期） | 是（运行时） |
| 类型集合 | 编译期确定 | 编译期确定 | 运行时任意 |
| 访问方式 | 直接成员 | `get`/`visit` | `any_cast` |
| 非平凡类型 | C11 起支持 | 完全支持 | 完全支持 |
| 性能 | 最小开销 | 索引 + 值存储 | 可能堆分配 |

## 最佳实践

- **优先使用 `overloaded + visit`** 而非 `if-else` 链式 `holds_alternative`。
- **确保类型有 `noexcept` 移动构造函数**，避免 `valueless_by_exception`。
- **使用 `monostate`** 作为第一类型实现可默认构造的 variant。
- **替代虚函数调用**：类型集合固定时，`variant + visit` 避免间接调用和堆分配。

## 常见陷阱

```cpp
// 陷阱 1：类型歧义
// std::variant<int, long> v = 42;  // 歧义
std::variant<int, long> v{42};       // OK：匹配 int

// 陷阱 2：默认初始化不是"空"
std::variant<int, std::string> v;
// v.index() == 0, std::get<0>(v) == 0
// 第一个类型必须可默认构造

// 陷阱 3：不可拷贝类型影响整个 variant
// variant<int, std::unique_ptr<int>> v;
// auto v2 = v;  // 错误：unique_ptr 不可拷贝

// 陷阱 4：不要用 string 字面量触发歧义
std::variant<std::string, const char*> v{"hi"}; // 选择 const char*
std::variant<std::string, const char*> v2{std::string{"hi"}}; // 显式指定 string
```
