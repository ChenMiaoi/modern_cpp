---
title: "std::function"
topic: unknown
feature: function
standard: N/A
status_checked_at: 2026-06-20
---
# std::function

## 概述

`std::function<R(Args...)>` 是 C++11 引入的类型擦除包装器，用于统一存储任意可调用对象（函数指针、lambda、`std::bind` 表达式、函数对象）。它是回调机制和函数式编程模式的核心组件。

## 头文件

```cpp
#include <functional>
```

## 语法

```cpp
std::function<R(Args...)> f;

// 示例
std::function<int(int, int)> add = [](int a, int b) { return a + b; };
std::function<void()> callback = []() { std::cout << "done\n"; };
```

## 可包装的类型

| 类型 | 示例 |
|------|------|
| 函数指针 | `int(*)(int, int)` |
| Lambda | `[](int x) { return x * 2; }` |
| `std::bind` 表达式 | `std::bind(add, _1, 10)` |
| 函数对象 | 结构体重载 `operator()` |
| 成员函数指针 | `&Class::method`（需绑定 `this`） |

## 代码示例

### 基本用法

```cpp
#include <functional>
#include <iostream>

void execute(std::function<int(int, int)> op, int a, int b) {
    std::cout << "Result: " << op(a, b) << "\n";
}

int main() {
    execute([](int a, int b) { return a + b; }, 3, 4);  // 7
    execute([](int a, int b) { return a * b; }, 3, 4);  // 12
}
```

### 排序自定义比较器

```cpp
#include <algorithm>
#include <functional>
#include <vector>

int main() {
    std::vector<int> nums = {5, 2, 8, 1, 9, 3};

    std::function<bool(int, int)> desc = [](int a, int b) {
        return a > b;
    };

    std::sort(nums.begin(), nums.end(), desc);
    // nums: {9, 8, 5, 3, 2, 1}
}
```

### 回调模式

```cpp
#include <functional>
#include <string>
#include <iostream>

class Button {
    std::function<void()> onClick_;
public:
    void setOnClick(std::function<void()> callback) {
        onClick_ = callback;
    }
    void click() {
        if (onClick_) onClick_();
    }
};

int main() {
    Button btn;
    int count = 0;
    btn.setOnClick([&count]() {
        count++;
        std::cout << "Clicked " << count << " times\n";
    });
    btn.click();  // Clicked 1 times
    btn.click();  // Clicked 2 times
}
```

### 存储不同类型

```cpp
#include <functional>
#include <iostream>

int add(int a, int b) { return a + b; }

struct Multiplier {
    int factor;
    int operator()(int x) const { return x * factor; }
};

int main() {
    std::function<int(int, int)> f1 = add;
    std::function<int(int)> f2 = Multiplier{5};

    std::cout << f1(3, 4) << "\n";  // 7
    std::cout << f2(6) << "\n";     // 30
}
```

## 性能考虑

| 方面 | 说明 |
|------|------|
| 堆分配 | 小型捕获可能内联，大捕获触发堆分配 |
| 虚分发 | 调用通过内部虚函数，有间接调用开销 |
| 与函数指针对比 | 函数指针零开销，`std::function` 有包装成本 |
| 与模板对比 | 模板完全内联，`std::function` 有类型擦除成本 |

**经验法则**：若性能是关键路径，优先使用模板。若需要运行时多态或类型擦除，使用 `std::function`。

## std::function vs 函数指针 vs 模板

| 特性 | 函数指针 | `std::function` | 模板 |
|------|---------|-----------------|------|
| 捕获状态 | 不能 | 能 | 能 |
| 类型擦除 | 否 | 是 | 否 |
| 性能开销 | 零 | 中等 | 零（内联） |
| 运行时多态 | 否 | 是 | 否 |
| 适用场景 | 纯函数 | 回调/策略 | 高性能泛型代码 |

## 注意事项与陷阱

**空 std::function**

```cpp
std::function<int()> f;
// f();  // 未定义行为，抛出 std::bad_function_call

if (f) {
    f();  // 安全
}
```

**移动语义**

`std::function` 支持移动语义，大捕获的 lambda 移动比拷贝更高效：

```cpp
std::function<void()> f = [big_data = std::move(data)]() {
    // 使用 big_data
};
```

**类型擦除的隐式成本**

`std::function` 会在内部存储一个指向捕获数据的指针。即使 lambda 为空捕获，也有间接调用开销。对于简单的函数指针场景，直接使用函数指针更高效。

## 编译器支持

| 编译器 | 最低版本 |
|--------|---------|
| GCC | 4.4+ |
| Clang | 3.1+ |
| MSVC | 2012+（VS 11.0） |
