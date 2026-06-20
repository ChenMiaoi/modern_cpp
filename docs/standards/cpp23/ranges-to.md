---
title: std::ranges::to
topic: cpp23
feature: ranges-to
standard: C++23
status_checked_at: 2026-06-20
standard_refs:
  - draft: N4950
    clause: "[range.utility]"
proposals:
  - paper: P1206
    revision: R7
    status: accepted
exercises: []
solutions: []
---
# std::ranges::to

C++23 引入 `std::ranges::to`，提供从任意范围（range）构造容器的通用机制。它解决了长期以来"无法优雅地将 view 转换为具体容器"的问题，是 Ranges 库中最实用的工具之一。

## 基本用法

```cpp
#include <ranges>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <string>

int main() {
    // 从初始化列表构造
    auto v = std::ranges::to<std::vector>({1, 2, 3, 4, 5});

    // 从另一个容器构造（类型转换）
    std::list<int> lst = {10, 20, 30};
    auto vec = std::ranges::to<std::vector>(lst);

    // 从 view 构造（最常见用法）
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto evens = nums | std::views::filter([](int n) { return n % 2 == 0; });
    auto result = std::ranges::to<std::vector>(evens);
    // result = {2, 4, 6, 8, 10}
}
```

## 支持的容器类型

```cpp
#include <ranges>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <string>

int main() {
    std::vector<int> src = {1, 2, 3};

    auto vec = std::ranges::to<std::vector>(src);
    auto lst = std::ranges::to<std::list>(src);
    auto deq = std::ranges::to<std::deque>(src);
    auto set = std::ranges::to<std::set>(src);
    auto uset = std::ranges::to<std::unordered_set>(src);
    auto str = std::ranges::to<std::string>(src | std::views::transform([](int i) {
        return static_cast<char>('0' + i);
    }));
}
```

## 与 std::from_ranges 构造器

`ranges::to` 依赖 C++23 新增的 ranges 构造器。每个容器都支持从 input_range 直接构造：

```cpp
#include <ranges>
#include <vector>
#include <set>

int main() {
    auto odds = std::views::iota(1, 11) | std::views::filter([](int n) {
        return n % 2 == 1;
    });

    // ranges::to 是语法糖，等价于调用 ranges 构造器
    std::vector<int> v1(odds);  // C++23 ranges 构造器
    auto v2 = std::ranges::to<std::vector>(odds);  // 更明确的意图
}
```

## 实际使用场景

### 收集过滤结果

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

struct Student {
    std::string name;
    int score;
};

int main() {
    std::vector<Student> students = {
        {"Alice", 90}, {"Bob", 65}, {"Charlie", 85},
        {"Diana", 72}, {"Eve", 95}
    };

    // 收集优秀学生的名字
    auto excellent_names = students
        | std::views::filter([](const Student& s) { return s.score >= 80; })
        | std::views::transform([](const Student& s) { return s.name; });

    auto names = std::ranges::to<std::vector<std::string>>(excellent_names);
    // names = {"Alice", "Charlie", "Eve"}
}
```

### 链式操作后收集

```cpp
#include <ranges>
#include <vector>
#include <set>
#include <iostream>

int main() {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};

    // 排序 + 去重 + 收集为 set（自动去重）
    auto result = data
        | std::views::transform([](int n) { return n * 2; })
        | std::ranges::to<std::set>();

    // result = {2, 4, 6, 8, 10, 12, 14, 16, 18}
}
```

### 构建关联容器

```cpp
#include <ranges>
#include <vector>
#include <map>
#include <string>
#include <iostream>

int main() {
    std::vector<std::pair<std::string, int>> pairs = {
        {"apple", 3}, {"banana", 5}, {"cherry", 2}
    };

    auto fruit_map = std::ranges::to<std::map>(pairs);
    // fruit_map = {{"apple":3}, {"banana":5}, {"cherry":2}}

    // 从 key-value view 构建
    auto keys = std::views::iota(1, 4);
    auto mapping = keys | std::views::transform([](int i) {
        return std::pair{i, i * i};
    });
    auto sq_map = std::ranges::to<std::map>(mapping);
    // sq_map = {{1:1}, {2:4}, {3:9}}
}
```

## 自定义容器支持

任何提供 ranges 构造器的自定义容器都可以与 `ranges::to` 配合使用：

```cpp
#include <ranges>
#include <vector>

template <typename T>
class MyBuffer {
    std::vector<T> data_;
public:
    template <std::ranges::input_range R>
    explicit MyBuffer(R&& range) : data_(std::ranges::begin(range), std::ranges::end(range)) {}

    const auto& data() const { return data_; }
};

// 启用 ranges::to 支持
template <typename T>
MyBuffer(std::ranges::input_range) -> MyBuffer<T>;

int main() {
    std::vector<int> src = {1, 2, 3, 4, 5};
    auto buf = std::ranges::to<MyBuffer>(src);
}
```

## 转发范围与 move 语义

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

int main() {
    // 支持 move-only 类型
    auto uptrs = std::views::iota(0, 5)
        | std::views::transform([](int i) {
            return std::make_unique<int>(i);
        });

    auto vec = std::ranges::to<std::vector<std::unique_ptr<int>>>(std::move(uptrs));
}
```

## 与旧写法对比

```cpp
#include <ranges>
#include <vector>
#include <algorithm>
#include <iterator>

int main() {
    std::vector<int> src = {1, 2, 3, 4, 5};
    auto view = src | std::views::filter([](int n) { return n > 2; });

    // C++20 旧写法（冗长、不直观）
    std::vector<int> old_way;
    std::ranges::copy(view, std::back_inserter(old_way));

    // C++23 ranges::to（简洁、意图明确）
    auto new_way = std::ranges::to<std::vector>(view);
}
```

## 编译器支持

| 编译器 | 版本 | 支持状态 |
|--------|------|----------|
| GCC | 14+ | 完整支持 |
| Clang | 17+ | 完整支持 |
| MSVC | 19.38 (VS 2022 17.8) | 完整支持 |

> **注意**：部分编译器可能需要 `-std=c++23` 或 `-std=c++2b` 标志。

## 注意事项

- `ranges::to` 要求目标容器支持 ranges 构造器（C++23 新增）
- 对于 `std::string`，`ranges::to` 将字符范围转换为字符串
- 容器大小可预知时（如已知 `ranges::size`），`ranges::to` 会预分配内存
- `ranges::to` 支持 `std::initializer_list` 作为参数
- 优先使用 `ranges::to` 而非手动 `std::ranges::copy` + `std::back_inserter` 模式
