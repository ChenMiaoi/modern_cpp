---
title: C++20 Ranges 算法
topic: unknown
feature: ranges-algorithms
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 Ranges 算法

## 概述

C++20 在 `<algorithm>` 中为几乎所有 STL 算法提供了 `std::ranges::` 命名空间版本。相比传统迭代器对接口，ranges 算法：

- 接受单一 range 对象，而非 `begin/end` 迭代器对。
- 支持**投影（projection）**：对元素的子字段进行比较/排序。
- 支持**哨兵（sentinel）**：`end` 可以是与 `begin` 不同类型的哨兵。
- 返回迭代器而非 void，便于链式操作。

## 常用算法

### `ranges::find`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5};
    auto it = std::ranges::find(v, 3);
    if (it != v.end()) {
        std::cout << "Found: " << *it << "\n";  // Found: 3
    }

    // 按条件查找
    auto it2 = std::ranges::find_if(v, [](int n) { return n > 3; });
    std::cout << "First > 3: " << *it2 << "\n";  // First > 3: 4
}
```

### `ranges::sort`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {5, 3, 1, 4, 2};
    std::ranges::sort(v);
    // v == {1, 2, 3, 4, 5}

    // 自定义比较
    std::ranges::sort(v, std::ranges::greater{});
    // v == {5, 4, 3, 2, 1}

    // 投影：按绝对值排序
    std::vector nums = {-3, 1, -2, 4, -5};
    std::ranges::sort(nums, {}, [](int n) { return std::abs(n); });
    // nums == {1, -2, -3, 4, -5}
}
```

### `ranges::transform`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5};
    std::vector<int> result;
    std::ranges::transform(v, std::back_inserter(result),
                           [](int n) { return n * n; });
    // result == {1, 4, 9, 16, 25}

    // 二元 transform
    std::vector a = {1, 2, 3};
    std::vector b = {10, 20, 30};
    std::vector<int> sum;
    std::ranges::transform(a, b, std::back_inserter(sum), std::plus{});
    // sum == {11, 22, 33}
}
```

### `ranges::filter`（通过 views）

```cpp
#include <ranges>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto evens = v | std::views::filter([](int n) { return n % 2 == 0; });
    for (int n : evens)
        std::cout << n << ' ';  // 2 4 6 8 10
}
```

### `ranges::for_each`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5};
    std::ranges::for_each(v, [](int& n) { n *= 2; });
    // v == {2, 4, 6, 8, 10}

    // 带投影
    struct Person { std::string name; int age; };
    std::vector<Person> people = {{"Alice", 30}, {"Bob", 25}};
    std::ranges::for_each(people, [](const Person& p) {
        std::cout << p.name << "\n";
    }, &Person::name);  // 投影到 name 字段
}
```

### `ranges::count_if`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto count = std::ranges::count_if(v, [](int n) { return n % 2 == 0; });
    std::cout << "Even count: " << count << "\n";  // Even count: 5
}
```

### `ranges::min` / `ranges::max`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {3, 1, 4, 1, 5, 9, 2, 6};
    std::cout << "Min: " << std::ranges::min(v) << "\n";  // Min: 1
    std::cout << "Max: " << std::ranges::max(v) << "\n";  // Max: 9

    // 按绝对值取最小
    std::vector nums = {-5, 3, -2, 4};
    auto min_abs = std::ranges::min(nums, {}, [](int n) { return std::abs(n); });
    std::cout << "Min abs: " << min_abs << "\n";  // Min abs: -2
}
```

## Projected Iterators（投影迭代器）

投影是 ranges 算法的核心特性：在比较或排序时，不直接比较元素，而是比较元素的投影值。

```cpp
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>

struct Employee {
    std::string name;
    int salary;
};

int main() {
    std::vector<Employee> employees = {
        {"Alice", 70000}, {"Bob", 50000}, {"Charlie", 60000}
    };

    // 按 salary 排序
    std::ranges::sort(employees, std::ranges::less{}, &Employee::salary);
    for (const auto& e : employees)
        std::cout << e.name << ": " << e.salary << "\n";
    // Bob: 50000
    // Charlie: 60000
    // Alice: 70000

    // 按 salary 查找
    auto it = std::ranges::find(employees, 60000, &Employee::salary);
    std::cout << "Found: " << it->name << "\n";  // Found: Charlie
}
```

## 与传统 STL 算法对比

| 维度 | 传统 STL | ranges 算法 |
|------|----------|-------------|
| 接口 | `std::sort(begin, end)` | `std::ranges::sort(range)` |
| 比较器 | `std::sort(begin, end, comp)` | `std::ranges::sort(range, comp, proj)` |
| 投影 | 不支持 | 原生支持 |
| 返回值 | void（大部分） | 迭代器（便于链式操作） |
| 哨兵 | 不支持 | 支持不同类型的哨兵 |
| 范围检查 | 不检查 | 通过 concept 约束类型安全 |

```cpp
// 传统方式
std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
    return a.salary < b.salary;
});

// ranges 方式
std::ranges::sort(v, {}, &Employee::salary);
```

## 管道组合与算法

ranges 算法可与 view 管道组合使用：

```cpp
#include <ranges>
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // 先 filter 再 transform 再排序
    auto result = v
        | std::views::filter([](int n) { return n % 2 == 0; })
        | std::views::transform([](int n) { return n * n; })
        | std::views::common;

    std::vector<int> sorted(result.begin(), result.end());
    std::ranges::sort(sorted);
    for (int n : sorted)
        std::cout << n << ' ';  // 4 16 36 64 100
}
```

## 所有支持投影的算法

以下 ranges 算法均支持投影参数：

| 算法 | 说明 |
|------|------|
| `ranges::sort` | 排序 |
| `ranges::stable_sort` | 稳定排序 |
| `ranges::partial_sort` | 部分排序 |
| `ranges::min` / `ranges::max` | 最小/最大值 |
| `ranges::minmax` | 最小最大值对 |
| `ranges::find` / `ranges::find_if` | 查找 |
| `ranges::count` / `ranges::count_if` | 计数 |
| `ranges::all_of` / `ranges::any_of` / `ranges::none_of` | 条件检查 |
| `ranges::remove_if` | 条件删除 |
| `ranges::unique` | 去重 |
| `ranges::adjacent_find` | 查找相邻重复 |
| `ranges::lower_bound` / `ranges::upper_bound` | 二分查找 |

## 编译器支持

| 编译器 | 版本 | 支持状态 |
|--------|------|----------|
| GCC | 10+ | 完整支持（`-std=c++20`） |
| Clang | 16+ | 完整支持（`-std=c++20`） |
| MSVC | 19.29+ (VS 2019 16.10+) | 完整支持（`/std:c++20`） |

```cpp
// 编译验证
// g++ -std=c++20 ranges_algo.cpp -o ranges_algo
// clang++ -std=c++20 ranges_algo.cpp -o ranges_algo
// cl.exe /std:c++20 ranges_algo.cpp
```

## 常见陷阱

```cpp
// 1. 投影不是 lambda——是函数指针或成员指针
std::ranges::sort(v, {}, &Employee::salary);  // 正确
std::ranges::sort(v, {}, [](const auto& e) { return e.salary; });  // 也可以

// 2. 返回值是迭代器，不是 range
auto it = std::ranges::find(v, 42);
// it 是迭代器，不是 range

// 3. 算法不修改 range 大小（大部分）
// ranges::sort 排序原 range
// ranges::transform 需要输出迭代器

// 4. 投影组合是笛卡尔积
// sort(comp, proj) 等价于 sort([comp, proj](a, b){ return comp(proj(a), proj(b)); })
```

## 总结

- ranges 算法接受 range 对象，比传统迭代器对接口更简洁。
- 投影（projection）是核心特性，支持对子字段排序/查找。
- 返回迭代器便于链式操作和与其他算法组合。
- 与 view 管道无缝集成，实现声明式数据处理。
- GCC 10+、Clang 16+、MSVC 19.29+ 均已支持。
