# C++17 std::any

## 概述

`std::any` 是 C++17 在 `<any>` 中引入的类型安全容器，可存储**任意可拷贝构造类型的单个值**。它在运行时跟踪实际类型，类型不匹配时通过 `std::bad_any_cast` 异常报告错误。本质是 `void*` 的类型安全替代品，适用于配置解析、脚本绑定、插件系统等类型不可预知的场景。

## 构造与赋值

```cpp
#include <any>
#include <string>

std::any a1;                          // 默认构造：空
std::any a2{42};                      // 值构造
std::any a3{std::string{"hello"}};

a1 = std::string{"world"};            // 赋值改变类型
a1 = 100;                              // 从 string 变为 int

std::any a4 = a2;                      // 深拷贝

// 原地构造
std::any a5{std::in_place_type<std::string>, 5, 'X'}; // "XXXXX"
```

`std::any` 要求存储的类型满足 `CopyConstructible`。不可拷贝的类型（如 `unique_ptr`）不能直接存储。

## std::any_cast：值访问

```cpp
std::any a{42};

// 值语义——类型不匹配时抛 std::bad_any_cast
int val = std::any_cast<int>(a);       // 42
// double d = std::any_cast<double>(a); // 抛异常

// 指针语义——不匹配时返回 nullptr，不抛异常
int* p = std::any_cast<int>(&a);
if (p) std::cout << *p << "\n";       // 42

double* dp = std::any_cast<double>(&a);
// dp == nullptr
```

`any_cast<T>(&any)` 接收 `any*`，返回 `T*`——这是避免异常的推荐方式。

## 状态查询

```cpp
std::any a;

a.has_value();             // false
a = 42;
a.has_value();             // true
a.type() == typeid(int);   // true
a.type().name();           // 平台相关类型名

a.reset();                 // 清空
a.has_value();             // false
```

## 异构容器

```cpp
#include <any>
#include <vector>
#include <iostream>

int main() {
    std::vector<std::any> bag;
    bag.push_back(42);
    bag.push_back(3.14);
    bag.push_back(std::string{"hello"});
    bag.push_back(true);

    for (const auto& item : bag) {
        if (item.type() == typeid(int))
            std::cout << "int: " << std::any_cast<int>(item) << "\n";
        else if (item.type() == typeid(std::string))
            std::cout << "string: " << std::any_cast<std::string>(item) << "\n";
    }
}
```

这是 `std::any` 最典型的使用场景：配置系统、消息传递、JSON 中间表示。

## 简易属性系统示例

```cpp
#include <any>
#include <string>
#include <unordered_map>

class Properties {
    std::unordered_map<std::string, std::any> data_;
public:
    template<typename T>
    void set(const std::string& key, T&& value) {
        data_[key] = std::forward<T>(value);
    }

    template<typename T>
    T get(const std::string& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) throw std::runtime_error("key not found");
        return std::any_cast<T>(it->second);
    }

    template<typename T>
    T get_or(const std::string& key, T default_val) const {
        auto it = data_.find(key);
        if (it == data_.end() || it->second.type() != typeid(T))
            return default_val;
        return std::any_cast<T>(it->second);
    }
};
```

## 与 std::variant 和 void* 的比较

| 特性 | `std::any` | `std::variant<Ts...>` | `void*` |
|------|-----------|----------------------|---------|
| 类型安全 | 运行时检查 | 编译期保证 | 无 |
| 可存储类型 | 任意可拷贝类型 | 固定类型列表 | 任意（仅指针） |
| 错误报告 | `bad_any_cast` | `bad_variant_access` | 无（UB） |
| 大小 | 指针 + 可能堆分配 | 值类型 + 索引 | 一个指针 |
| 适用场景 | 类型不可预知 | 类型集合已知 | 底层系统编程 |

## 小对象优化（SBO）

大多数实现对 `std::any` 采用小对象优化——小对象（通常 ≤16~24 字节）直接存储在 `any` 内部，无堆分配；大对象触发堆分配。

```cpp
std::any a{42};                          // 通常无堆分配
std::any b{std::vector<int>(10000, 1)};  // 堆分配
```

## 最佳实践

- **仅在类型真正不可预知时使用 `std::any`**。类型集合已知时优先用 `std::variant`。
- **使用指针版 `any_cast`**（`any_cast<T>(&a)`）避免异常开销。
- **存储大对象时考虑 `shared_ptr<T>`** 减少拷贝开销。
- **记录属性值的预期类型**——类型契约存在于调用者脑中而非类型系统中。

## 常见陷阱

```cpp
// 陷阱 1：类型不匹配
std::any a{42};
// std::any_cast<long>(a);  // 抛 bad_any_cast！int != long

// 陷阱 2：存储指针 vs 存储值
std::any a1{new int{42}};   // 存储 int*，any_cast 用 int*
std::any a2{42};            // 存储 int，any_cast 用 int
// 两者不同——不要混淆

// 陷阱 3：const 限定
const std::any ca{42};
// std::any_cast<int&>(ca);  // 抛异常！不能转为非 const 引用
int val = std::any_cast<int>(ca);           // OK：值拷贝
const int& cr = std::any_cast<const int&>(ca); // OK

// 陷阱 4：不可拷贝类型
// std::any a = std::make_unique<int>(42);  // 编译错误
std::any a = std::make_shared<int>(42);     // OK：shared_ptr 可拷贝

// 陷阱 5：性能敏感路径
// any 的 any_cast 涉及运行时 typeid 比较
// 热路径上优先使用 std::variant
```
