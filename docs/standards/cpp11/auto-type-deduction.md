# auto 类型推导

## 概述

`auto` 关键字让编译器从初始化表达式中推导变量的类型，减少冗余的类型声明。

## 基本用法

```cpp
auto i = 42;            // int
auto d = 3.14;          // double
auto s = std::string("hello");  // std::string
auto it = vec.begin();  // std::vector<int>::iterator
```

## 推导规则

`auto` 使用模板参数推导的规则（丢弃引用和顶层 `const`）：

```cpp
const int ci = 10;
auto a = ci;      // int（丢弃顶层 const）

int& ri = i;
auto b = ri;      // int（丢弃引用）

const int& cri = ci;
auto c = cri;     // int（丢弃引用和顶层 const）
```

### 保留底层 const 和引用

使用 `auto&` 或 `const auto&` 保留引用：

```cpp
const int ci = 10;
auto& d = ci;         // const int&（保留底层 const）

int i = 42;
auto& e = i;          // int&

const int& cri = ci;
auto& f = cri;        // const int&（保留引用和底层 const）
```

## 使用场景

### 迭代器

```cpp
// C++11 之前：
for (std::vector<std::pair<int, std::string>>::const_iterator it = vec.begin();
     it != vec.end(); ++it) { ... }

// C++11：
for (auto it = vec.begin(); it != vec.end(); ++it) { ... }

// C++11 range-for（更好的选择）：
for (const auto& [key, value] : vec) { ... }  // C++17 结构化绑定
```

### 模板返回值

```cpp
template<typename T, typename U>
auto add(T t, U u) -> decltype(t + u) {  // C++11 尾置返回类型
    return t + u;
}
```

### 复杂类型

```cpp
std::map<std::string, std::vector<int>> data;
auto& bucket = data["key"];  // std::vector<int>&
```

## 注意事项

### 不可用于函数参数（C++11/14）

```cpp
// C++11/14 不允许：
void foo(auto x);  // 错误

// C++20 Concepts 允许：
void foo(auto x);  // 等价于 template<typename T> void foo(T x);
```

### 数组和引用的退化

```cpp
int arr[5] = {1, 2, 3, 4, 5};
auto a = arr;       // int*（数组退化为指针）
auto& b = arr;      // int(&)[5]（引用保留数组类型）
```

### 一致性初始化

```cpp
auto x = {1, 2, 3};  // std::initializer_list<int>！
auto y{42};           // C++11: std::initializer_list<int>
                      // C++17: int（规则改变）
```

## 最佳实践

- **优先使用 `auto`**：减少冗余，避免窄化，适应泛型代码
- **显式类型更清晰时不用 `auto`**：当类型影响可读性时（如字面量 `0` 的类型不明确）
- **范围 for 用 `const auto&`**：避免拷贝，明确不可变意图
- **智能指针和迭代器**：几乎总是应该用 `auto`

## 与其他特性的关系

| 特性 | 关系 |
|------|------|
| `decltype` | 获取表达式的精确类型（含引用） |
| `decltype(auto)` | C++14，结合两者的优点 |
| 结构化绑定 | C++17，`auto [a, b] = pair;` |
| CTAD | C++17，类模板参数推导 |
