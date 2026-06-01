# std::generator

`std::generator` 是 C++23 提供的标准协程生成器，用于惰性生成元素序列。它封装了 C++20 协程原语，提供开箱即用的生成器类型。

## 基本用法

```cpp
#include <generator>
#include <iostream>
#include <ranges>

std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}

int main() {
    for (int fib : fibonacci() | std::views::take(10)) {
        std::cout << fib << " ";
    }
    // 0 1 1 2 3 5 8 13 21 34
}
```

## co_yield 与 co_return

```cpp
std::generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;          // 产出一个值
    }
    // 隐式 co_return
}

std::generator<int> limited() {
    co_yield 1;
    co_yield 2;
    co_yield 3;
    co_return;               // 显式结束
}
```

## 递归生成器

使用 `std::ranges::elements_of` 递归产出另一个生成器的所有元素：

```cpp
#include <generator>
#include <iostream>

struct Tree {
    int value;
    Tree* left = nullptr;
    Tree* right = nullptr;
};

std::generator<int> inorder(Tree* node) {
    if (!node) co_return;
    co_yield std::ranges::elements_of(inorder(node->left));
    co_yield node->value;
    co_yield std::ranges::elements_of(inorder(node->right));
}

int main() {
    Tree d{4}, e{5}, f{6};
    Tree b{2, &d, &e}, c{3, &f, nullptr};
    Tree a{1, &b, &c};

    for (int v : inorder(&a)) {
        std::cout << v << " ";
    }
    // 4 2 5 1 6 3
}
```

## 与手写协程对比

C++20 需要手动实现 `promise_type`、迭代器等约 50-100 行样板代码：

```cpp
// C++20 手写（需实现 promise_type、迭代器、析构、移动语义…）
class manual_generator {
public:
    struct promise_type {
        int current_value;
        std::suspend_always yield_value(int v) { current_value = v; return {}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        manual_generator get_return_object() { /* ... */ }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    // 迭代器实现、生命周期管理...
};

// C++23 — 一行搞定
std::generator<int> gen() { co_yield 1; }
```

## 与 ranges 配合

`std::generator` 满足 `std::ranges::input_range`，可直接用于所有 range 操作：

```cpp
std::generator<int> naturals() {
    for (int i = 0; ; ++i) co_yield i;
}

auto evens = naturals()
    | std::views::filter([](int n) { return n % 2 == 0; })
    | std::views::take(5);
// 0, 2, 4, 6, 8

auto squares = naturals()
    | std::views::take(5)
    | std::views::transform([](int n) { return n * n; });
// 0, 1, 4, 9, 16
```

## 实用模式

```cpp
// 组合多个生成器
std::generator<int> concat(std::generator<int> a, std::generator<int> b) {
    co_yield std::ranges::elements_of(std::move(a));
    co_yield std::ranges::elements_of(std::move(b));
}

// 展平嵌套结构
std::generator<int> flatten(const std::vector<std::vector<int>>& vecs) {
    for (const auto& vec : vecs)
        for (int v : vec) co_yield v;
}
```

## C++26: std::execution

C++26 计划引入 `std::execution`（P2300），其中的 sender/receiver 模型支持异步生成和消费序列。`std::generator` 是同步惰性序列，而 `std::execution` 扩展到异步并发场景。

## 注意事项

- 迭代器是输入迭代器（input iterator），只可单次遍历
- 生成器对象不可拷贝（move-only）
- 协程帧在堆上分配，高频短生成器需注意分配开销
- 异常会传播给消费者
