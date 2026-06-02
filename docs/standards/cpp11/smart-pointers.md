---
title: "智能指针"
topic: unknown
feature: smart-pointers
standard: N/A
status_checked_at: 2026-06-02
---
# 智能指针

## 概述

C++11 用三种智能指针取代了 `auto_ptr`，建立了清晰的资源所有权语义：

| 智能指针 | 所有权 | 用途 |
|----------|--------|------|
| `unique_ptr` | 独占 | 默认选择，零开销 |
| `shared_ptr` | 共享 | 多个拥有者共同管理同一资源 |
| `weak_ptr` | 观察 | 打破 `shared_ptr` 的循环引用 |

## `std::unique_ptr`

独占所有权，不可拷贝，只能移动。开销与裸指针相同（零额外内存，零运行时开销）。

```cpp
#include <memory>

// 创建
auto p = std::make_unique<int>(42);             // 推荐
auto arr = std::make_unique<int[]>(10);          // 数组版本

// 使用
std::cout << *p << '\n';      // 解引用：42
std::cout << p->method();     // 成员访问

// 移动（所有权转移）
auto p2 = std::move(p);       // p 变为 nullptr
assert(p == nullptr);

// 自定义删除器
auto file = std::unique_ptr<FILE, decltype(&fclose)>(
    fopen("data.txt", "r"), &fclose);
```

### 何时使用

- 默认选择，除非有明确的共享需求
- 工厂函数返回值
- Pimpl 惯用法中的成员
- 容器中存储多态对象

## `std::shared_ptr`

共享所有权，内部维护一个引用计数。当最后一个 `shared_ptr` 被销毁时释放资源。

```cpp
auto p1 = std::make_shared<int>(42);  // 引用计数 = 1
auto p2 = p1;                          // 引用计数 = 2
auto p3 = p1;                          // 引用计数 = 3

p2.reset();  // 引用计数 = 2
p3.reset();  // 引用计数 = 1

std::cout << p1.use_count();  // 1
```

### 开销

- **内存**：额外的控制块（引用计数 + 弱引用计数 + 删除器 + 分配器）≈ 16-32 字节
- **操作**：引用计数的原子增减（线程安全但有开销）

### `make_shared` vs 直接构造

```cpp
// 推荐：一次内存分配（对象和控制块一起）
auto p = std::make_shared<Widget>(args...);

// 两次内存分配（对象一次，控制块一次）
std::shared_ptr<Widget> p(new Widget(args...));
```

## `std::weak_ptr`

`weak_ptr` 观察 `shared_ptr` 管理的对象但不增加引用计数，用于打破循环引用。

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // 使用 weak_ptr 打破循环
};

auto n1 = std::make_shared<Node>();
auto n2 = std::make_shared<Node>();
n1->next = n2;
n2->prev = n1;  // 不增加 n1 的引用计数

// 使用前必须检查是否还活着
if (auto sp = n2->prev.lock()) {
    // sp 是 shared_ptr，此时 n1 保证存活
}
```

### `expired()` 检查

```cpp
std::weak_ptr<int> wp;
{
    auto sp = std::make_shared<int>(42);
    wp = sp;
    assert(!wp.expired());  // 还活着
}
assert(wp.expired());  // 已销毁
```

## 循环引用问题

```cpp
// 经典错误：
struct Parent {
    std::shared_ptr<Child> child;
};
struct Child {
    std::shared_ptr<Parent> parent;  // 循环引用！两者都不会被释放
};

// 正确：
struct Child {
    std::weak_ptr<Parent> parent;  // 使用 weak_ptr
};
```

## 自定义删除器

```cpp
// unique_ptr 的删除器是类型的一部分
std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(
    sqlite3_open("test.db"), &sqlite3_close);

// shared_ptr 的删除器不是类型的一部分
std::shared_ptr<FILE> fp(fopen("test.txt", "r"), &fclose);
```

## 最佳实践

| 场景 | 选择 |
|------|------|
| 默认 | `unique_ptr` |
| 多个拥有者 | `shared_ptr` |
| 缓存/观察 | `weak_ptr` |
| 工厂函数 | 返回 `unique_ptr`（调用方可隐式转为 `shared_ptr`） |
| 已有裸指针 | 不要从裸指针构造 `shared_ptr`，用 `make_shared` 创建 |
| 容器中存储多态 | `vector<unique_ptr<Base>>` |
