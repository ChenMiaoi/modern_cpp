---
title: "C++20"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++20

C++20（ISO/IEC 14882:2020）是继 C++11 之后最具影响力的版本，引入了四大"基石级"特性：**Concepts**、**Ranges**、**Coroutines** 和 **Modules**。

## 四大基石

### Concepts（概念）

对模板参数施加命名约束，替代 SFINAE 和 `static_assert`：

```cpp
template<typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
};

void sort(Sortable auto& container);
```

### Ranges（范围库）

基于惰性求值的管道式数据处理：

```cpp
auto result = numbers
    | views::filter([](int n) { return n > 0; })
    | views::transform([](int n) { return n * n; })
    | views::take(10);
```

### Coroutines（协程）

无栈协程的底层支持（`co_await`、`co_yield`、`co_return`），为异步编程和生成器模式提供语言级支持。标准库中的高级封装（`std::generator`）留给了 C++23。

### Modules（模块）

替代 `#include` 的模块化编译系统，解决头文件的编译效率和宏污染问题。

## 其他重要特性

| 特性 | 说明 |
|------|------|
| `<=>` 三路比较运算符 | 自动生成比较运算符 |
| `consteval` / `constinit` | 更精确的编译期语义 |
| `std::format` | 类型安全的格式化库（Python 风格） |
| `std::span` | 连续内存的非拥有序列视图 |
| `std::jthread` | 自动 join 的线程 |
| `std::latch` / `std::barrier` / `std::counting_semaphore` | 同步原语 |
| `std::source_location` | 替代 `__FILE__`/`__LINE__` |
| `std::ranges` 算法 | 范围化的查找、排序、变换 |
| `std::erase` / `std::erase_if` | 统一容器擦除 |
| 日历与时间区 | `chrono` 库的重大扩展 |

## 编译器支持

| 编译器 | 支持状态 |
|--------|---------|
| GCC | 10+（核心特性），12+ 较完整 |
| Clang | 15+（Modules 仍实验性） |
| MSVC | VS 2022 (17.0)+ |

## 延伸阅读

- [Concepts](/standards/cpp20/concepts)
- [Ranges](/standards/cpp20/ranges)
- [Coroutines](/standards/cpp20/coroutines)
- [Modules](/standards/cpp20/modules)
