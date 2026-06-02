---
title: "C++14 std::make_unique"
topic: unknown
feature: make-unique
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 std::make_unique

## 概述

C++11 引入了 `std::unique_ptr` 和 `std::make_shared`，但遗漏了 `std::make_unique`。C++14 补上了这个缺口。`std::make_unique` 在堆上构造对象并返回 `unique_ptr`，提供了异常安全的单表达式构造方式。它同时支持标量类型和数组类型。

## 语法

```cpp
// 标量版本
auto ptr = std::make_unique<T>(args...);

// 数组版本（C++14 新增）
auto arr = std::make_unique<T[]>(n);
```

头文件：`<memory>`

## `new` + `unique_ptr` vs `make_unique`

### C++11 方式（有隐患）

```cpp
// 问题 1：两次求值 — 可能泄漏
process(std::unique_ptr<Widget>(new Widget()), compute_priority());
// 若求值顺序为：
//   1. new Widget()
//   2. compute_priority() 抛异常
//   3. unique_ptr 构造未执行 → 内存泄漏

// 问题 2：不够简洁
auto p = std::unique_ptr<Widget>(new Widget(42, "hello"));
```

### C++14 方式

```cpp
// 异常安全：单次求值
process(std::make_unique<Widget>(), compute_priority());

// 简洁
auto p = std::make_unique<Widget>(42, "hello");
```

## 代码示例

### 基本用法

```cpp
#include <memory>
#include <string>
#include <iostream>

struct User {
    std::string name;
    int age;

    User(std::string n, int a) : name(std::move(n)), age(a) {}
};

int main() {
    // 构造参数完美转发
    auto user = std::make_unique<User>("Alice", 30);

    std::cout << user->name << ", " << user->age << '\n';
    // 输出: Alice, 30
}
```

### 数组版本

```cpp
#include <memory>

// C++11 只能用 new[]，不支持 make_unique 数组
// C++14 可以：
auto arr = std::make_unique<int[]>(10);  // 10 个 int，值初始化为 0

// 访问
arr[0] = 42;
arr[9] = 99;

// 注意：数组版本不能传构造参数
// auto arr2 = std::make_unique<int[]>(5, 1, 2, 3, 4, 5); // 错误
```

### 异常安全的关键场景

```cpp
#include <memory>

struct A { A() {} };
struct B { B() {} throw_on_copy{} };

// 双参数表达式求值交错可能导致泄漏
void unsafe(A*, int) {}
void safe(std::unique_ptr<A>, int) {}

int might_throw();

void demo() {
    // 不安全 — new A 和 might_throw() 的求值顺序未指定
    // unsafe(new A(), might_throw());

    // 安全 — make_unique 是一个完整的求值步骤
    safe(std::make_unique<A>(), might_throw());
}
```

### 与工厂模式配合

```cpp
#include <memory>
#include <string>

struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct Circle : Shape {
    double radius;
    explicit Circle(double r) : radius(r) {}
    double area() const override { return 3.14159 * radius * radius; }
};

struct Rect : Shape {
    double w, h;
    Rect(double w, double h) : w(w), h(h) {}
    double area() const override { return w * h; }
};

// 工厂函数
std::unique_ptr<Shape> make_shape(const std::string& type) {
    if (type == "circle") return std::make_unique<Circle>(5.0);
    if (type == "rect")   return std::make_unique<Rect>(3.0, 4.0);
    return nullptr;
}
```

### 为什么 C++11 遗漏了 `make_unique`

```cpp
// C++11 标准委员会的考虑：
// - make_shared 是性能必须的（单次内存分配）
// - make_unique 没有性能优势（仍需两次分配：控制块无需求）
// - 但遗漏 make_unique 导致用户被迫写 new + unique_ptr，破坏了异常安全
//
// Herb Sutter 和 Stephan T. Lavavej 在 C++14 提案中补上了这一缺口

// C++13 的 workaround（自己实现）：
template <typename T, typename... Args>
std::unique_ptr<T> make_unique_workaround(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

## 不适合使用 `make_unique` 的场景

```cpp
// 1. 需要自定义删除器
auto file = std::unique_ptr<FILE, decltype(&fclose)>(
    fopen("test.txt", "r"), &fclose);
// make_unique 不支持自定义删除器

// 2. 需要大块 raw 内存（placement new 风格）
auto buf = std::make_unique<char[]>(4096);  // 值初始化为 0，有开销
// 如需未初始化内存，仍需 new

// 3. 需要 aggregate initialization（C++14 不支持）
struct Point { int x; int y; };
// auto p = std::make_unique<Point>({1, 2});  // C++14 不允许
// C++20 起支持
```

## 最佳实践

1. **始终使用 `make_unique` 替代 `new` + `unique_ptr`**：除了需要自定义删除器的场景。
2. **理解异常安全本质**：`make_unique` 将对象构造和指针包装在一个表达式中，避免了求值顺序导致的泄漏。
3. **数组版本只支持默认初始化**：`make_unique<T[]>(n)` 值初始化元素；如需未初始化分配，仍需 `new`。
4. **不要对 `shared_ptr` 混用 `make_unique`**：`make_unique` 创建 `unique_ptr`，转入 `shared_ptr` 时使用 `std::shared_ptr<T>(std::move(uptr))`。
5. **配合 `auto` 推导**：`auto p = std::make_unique<T>(...)` 最简洁，避免重复类型名。
6. **C++20 改进**：C++20 支持 `make_unique` 的聚合初始化版本，可构造没有自定义构造函数的 POD 类型。
