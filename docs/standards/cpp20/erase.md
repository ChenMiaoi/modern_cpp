---
title: C++20 std::erase / std::erase_if
topic: unknown
feature: erase
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 `std::erase` / `std::erase_if`

## 概述

C++20 引入统一的容器擦除函数 `std::erase` 和 `std::erase_if`，取代繁琐的 Erase-Remove 惯用法。这些函数作为非成员函数提供，适用于所有标准容器。

核心优势：
- **简洁**：一行代码替代 `container.erase(std::remove(...), container.end())`。
- **统一**：所有容器使用相同接口。
- **安全**：避免迭代器失效的常见错误。

## `std::erase`——按值擦除

```cpp
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 2, 5, 2, 7};
    auto count = std::erase(v, 2);  // 删除所有值为 2 的元素
    std::cout << "Removed " << count << " elements\n";  // Removed 3 elements
    for (int n : v)
        std::cout << n << ' ';  // 1 3 5 7
}
```

## `std::erase_if`——按谓词擦除

```cpp
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto count = std::erase_if(v, [](int n) { return n % 2 == 0; });
    std::cout << "Removed " << count << " elements\n";  // Removed 5 elements
    for (int n : v)
        std::cout << n << ' ';  // 1 3 5 7 9
}
```

## 支持的容器

### `std::vector`

```cpp
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 2, 5};
    std::erase(v, 2);
    // v == {1, 3, 5}

    std::erase_if(v, [](int n) { return n > 3; });
    // v == {1, 3}
}
```

### `std::string`

```cpp
#include <string>
#include <iostream>

int main() {
    std::string s = "Hello, World!";
    std::erase(s, 'l');  // 删除所有 'l'
    std::cout << s << "\n";  // Heo, Word!

    std::erase_if(s, [](char c) { return std::isspace(c); });
    std::cout << s << "\n";  // Heo,Word!
}
```

### `std::list`

```cpp
#include <list>
#include <iostream>

int main() {
    std::list l = {1, 2, 3, 2, 5};
    std::erase(l, 2);
    // l == {1, 3, 5}

    std::erase_if(l, [](int n) { return n < 3; });
    // l == {3, 5}
}
```

### `std::forward_list`

```cpp
#include <forward_list>
#include <iostream>

int main() {
    std::forward_list fl = {1, 2, 3, 2, 5};
    std::erase(fl, 2);
    // fl == {1, 3, 5}

    std::erase_if(fl, [](int n) { return n > 3; });
    // fl == {1, 3}
}
```

### `std::deque`

```cpp
#include <deque>
#include <iostream>

int main() {
    std::deque d = {1, 2, 3, 2, 5};
    std::erase(d, 2);
    // d == {1, 3, 5}

    std::erase_if(d, [](int n) { return n < 4; });
    // d == {5}
}
```

### `std::map` / `std::set`

```cpp
#include <map>
#include <set>
#include <iostream>

int main() {
    // map
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::erase_if(m, [](const auto& pair) { return pair.second > 1; });
    // m == {{"a", 1}}

    // set
    std::set s = {1, 2, 3, 4, 5};
    std::erase_if(s, [](int n) { return n % 2 == 0; });
    // s == {1, 3, 5}
}
```

## 与 Erase-Remove 惯用法对比

```cpp
#include <vector>
#include <algorithm>
#include <iostream>

int main() {
    // 旧方式：Erase-Remove 惯用法
    {
        std::vector v = {1, 2, 3, 2, 5, 2, 7};
        v.erase(std::remove(v.begin(), v.end(), 2), v.end());
        // v == {1, 3, 5, 7}
    }

    // 新方式：std::erase
    {
        std::vector v = {1, 2, 3, 2, 5, 2, 7};
        std::erase(v, 2);
        // v == {1, 3, 5, 7}
    }

    // 旧方式：按条件删除
    {
        std::vector v = {1, 2, 3, 4, 5, 6};
        v.erase(std::remove_if(v.begin(), v.end(),
                [](int n) { return n % 2 == 0; }), v.end());
    }

    // 新方式：std::erase_if
    {
        std::vector v = {1, 2, 3, 4, 5, 6};
        std::erase_if(v, [](int n) { return n % 2 == 0; });
    }
}
```

### 对比表

| 维度 | Erase-Remove | `std::erase` |
|------|-------------|--------------|
| 代码量 | 2 行 | 1 行 |
| 可读性 | 低（需理解惯用法） | 高（自描述） |
| 错误风险 | 迭代器失效、边界错误 | 无 |
| 返回值 | void | 擦除的元素数量 |
| 容器支持 | 仅支持 `random_access` + `erase` | 所有容器 |

## 实际应用

### 日志过滤

```cpp
#include <vector>
#include <string>
#include <iostream>

void filter_logs(std::vector<std::string>& logs) {
    // 移除空日志
    std::erase_if(logs, [](const std::string& s) { return s.empty(); });
    // 移除调试日志
    std::erase_if(logs, [](const std::string& s) {
        return s.find("[DEBUG]") != std::string::npos;
    });
}
```

### 数据清洗

```cpp
#include <vector>
#include <algorithm>
#include <iostream>

struct SensorReading {
    double value;
    bool valid;
};

void clean_data(std::vector<SensorReading>& readings) {
    // 移除无效读数
    std::erase_if(readings, [](const SensorReading& r) { return !r.valid; });
    // 移除异常值
    std::erase_if(readings, [](const SensorReading& r) {
        return std::abs(r.value) > 1000.0;
    });
}
```

### 配置清理

```cpp
#include <map>
#include <string>
#include <iostream>

void clean_config(std::map<std::string, std::string>& config) {
    // 移除空值配置
    std::erase_if(config, [](const auto& pair) {
        return pair.second.empty();
    });
    // 移除注释配置（以 # 开头的 key）
    std::erase_if(config, [](const auto& pair) {
        return !pair.first.empty() && pair.first[0] == '#';
    });
}
```

## 编译器支持

| 编译器 | 版本 | 支持状态 |
|--------|------|----------|
| GCC | 10+ | 完整支持（`-std=c++20`） |
| Clang | 16+ | 完整支持（`-std=c++20`） |
| MSVC | 19.29+ (VS 2019 16.10+) | 完整支持（`/std:c++20`） |

```cpp
// 编译验证
// g++ -std=c++20 erase.cpp -o erase
// clang++ -std=c++20 erase.cpp -o erase
// cl.exe /std:c++20 erase.cpp
```

## 常见陷阱

```cpp
// 1. std::erase 不是容器成员函数——是自由函数
v.erase(2);           // 错误：vector::erase 需要迭代器
std::erase(v, 2);     // 正确

// 2. erase_if 的谓词接收 const 引用（关联容器）
std::map m = {{"a", 1}};
std::erase_if(m, [](const auto& pair) { return pair.second == 1; });

// 3. 返回值是 size_type（擦除的元素数量）
auto count = std::erase(v, 2);
// count 类型取决于容器（vector::size_type 等）

// 4. 不适用于数组
int arr[] = {1, 2, 3};
// std::erase(arr, 2);  // 编译错误
```

## 总结

- `std::erase` 和 `std::erase_if` 是 Erase-Remove 惯用法的现代替代。
- 一行代码替代两行，更简洁、更安全、更易读。
- 适用于所有标准容器：vector、string、list、deque、map、set 等。
- 返回擦除的元素数量，便于调试和日志。
- GCC 10+、Clang 16+、MSVC 19.29+ 均已支持。
