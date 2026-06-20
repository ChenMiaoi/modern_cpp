---
title: std::views::zip
topic: cpp23
feature: views-zip
standard: C++23
status_checked_at: 2026-06-20
standard_refs:
  - draft: N4950
    clause: "[range.zip]"
proposals:
  - paper: P2328
    revision: R4
    status: accepted
exercises: []
solutions: []
---
# std::views::zip

C++23 引入 `std::views::zip` 和 `std::views::zip_transform`，允许将多个范围组合为一个 tuple-like 元素的范围。这解决了长期存在的"并行迭代多个容器"的难题，避免了手动索引管理的复杂性。

## 基本用法

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>

int main() {
    std::vector<int> ids = {1, 2, 3};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    std::vector<double> scores = {90.5, 85.0, 92.3};

    // zip 将多个范围组合为 tuple-like 元素
    for (const auto& [id, name, score] : std::views::zip(ids, names, scores)) {
        std::cout << id << ": " << name << " = " << score << "\n";
    }
    // 输出:
    // 1: Alice = 90.5
    // 2: Bob = 85
    // 3: Charlie = 92.3
}
```

## zip_transform

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    std::vector<int> a = {1, 2, 3, 4};
    std::vector<int> b = {10, 20, 30, 40};

    // zip_transform 对每对元素应用函数
    auto sums = std::views::zip_transform(std::plus{}, a, b);
    for (int s : sums) {
        std::cout << s << " ";
    }
    // 输出: 11 22 33 44

    // 自定义函数
    auto products = std::views::zip_transform(
        [](int x, int y) { return x * y; }, a, b
    );
    for (int p : products) {
        std::cout << p << " ";
    }
    // 输出: 10 40 90 160
}
```

## 与手动索引对比

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

int main() {
    std::vector<std::string> first_names = {"John", "Jane", "Bob"};
    std::vector<std::string> last_names = {"Doe", "Smith", "Johnson"};
    std::vector<int> ages = {30, 25, 35};

    // 旧写法：手动管理索引，容易出错
    for (size_t i = 0; i < first_names.size(); ++i) {
        std::cout << first_names[i] << " " << last_names[i]
                  << " (age " << ages[i] << ")\n";
    }

    // C++23 新写法：清晰、安全、自动处理长度
    for (const auto& [first, last, age] : std::views::zip(first_names, last_names, ages)) {
        std::cout << first << " " << last << " (age " << age << ")\n";
    }
}
```

## 修改元素

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <tuple>

int main() {
    std::vector<int> keys = {1, 2, 3};
    std::vector<std::string> values = {"a", "b", "c"};

    // zip 返回引用，可以原地修改
    for (auto&& [k, v] : std::views::zip(keys, values)) {
        k *= 10;
        v += "_modified";
    }

    // keys = {10, 20, 30}
    // values = {"a_modified", "b_modified", "c_modified"}
}
```

## 常见模式

### 并行排序

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

int main() {
    std::vector<int> scores = {85, 92, 78, 95, 88};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "Diana", "Eve"};

    // 按分数排序，同时保持名字同步
    auto zipped = std::views::zip(scores, names);
    std::ranges::sort(zipped, std::ranges::greater{}, std::get<0>);

    for (const auto& [score, name] : std::views::zip(scores, names)) {
        std::cout << name << ": " << score << "\n";
    }
    // Diana: 95
    // Bob: 92
    // Alice: 85
    // Eve: 88
    // Charlie: 78
}
```

### 两个范围的集合操作

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    std::vector<int> x = {1, 2, 3, 4, 5};
    std::vector<int> y = {2, 4, 6, 8, 10};

    // 找出两个范围中相同索引处都为偶数的位置
    auto even_pairs = std::views::zip(x, y)
        | std::views::filter([](const auto& pair) {
            return std::get<0>(pair) % 2 == 0 && std::get<1>(pair) % 2 == 0;
        });

    for (const auto& [a, b] : even_pairs) {
        std::cout << "(" << a << ", " << b << ") ";
    }
    // 输出: (2, 4) (4, 8)
}
```

### 构建结构化数据

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

struct Record {
    int id;
    std::string name;
    double value;
};

int main() {
    std::vector<int> ids = {1, 2, 3};
    std::vector<std::string> names = {"sensor_A", "sensor_B", "sensor_C"};
    std::vector<double> values = {23.5, 19.8, 31.2};

    std::vector<Record> records;
    for (auto&& [id, name, val] : std::views::zip(ids, names, values)) {
        records.push_back({id, std::move(name), val});
    }
}
```

## 与 std::views::zip_view 类型

```cpp
#include <ranges>
#include <vector>
#include <typeinfo>
#include <iostream>

int main() {
    std::vector<int> a = {1, 2};
    std::vector<double> b = {1.1, 2.2};

    auto z = std::views::zip(a, b);
    // z 的类型: std::ranges::zip_view<std::vector<int>, std::vector<double>>
    // 每个元素类型: std::tuple<int&, double&>

    std::cout << typeid(z).name() << "\n";
}
```

## empty 范围处理

```cpp
#include <ranges>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b;  // 空范围

    // zip 的结果以最短范围为准
    auto z = std::views::zip(a, b);
    // z 为空范围（长度为 0）

    for (const auto& [x, y] : z) {
        std::cout << x << " " << y << "\n";
    }
    // 不会执行
}
```

## 编译器支持

| 编译器 | 版本 | 支持状态 |
|--------|------|----------|
| GCC | 14+ | 完整支持 |
| Clang | 17+ | 完整支持 |
| MSVC | 19.36 (VS 2022 17.6) | 完整支持 |

> **注意**：MSVC 需要 `/std:c++latest` 或 `/std:c++23` 标志。

## 注意事项

- `zip` 以最短范围的长度为准，多余的元素被忽略
- `zip` 返回的元素是 tuple-like 的引用（`std::tuple<Ts&...>`）
- `zip_transform` 接受可调用对象作为第一个参数
- 不能 zip 不同长度的范围并期望自动填充（使用 `std::views::counted` 或 `std::views::take`）
- 对 `zip` 的排序会同步修改所有关联范围
- `std::views::zip` 只接受 input_range 或更高级别的范围
