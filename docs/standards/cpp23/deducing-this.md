---
title: Deducing this（推导 this）
topic: cpp23
feature: deducing-this
standard: C++23
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp23/deducingthis1.cpp
solutions:
  - exercises/solutions/deducingthis1.cpp
---
# Deducing this（推导 this）

C++23 引入显式对象参数（explicit object parameter），允许通过 `this` 参数的类型推导来简化 CRTP、消除引用限定重载的重复代码，并支持递归 lambda。

## 基本语法

```cpp
struct Widget {
    // 传统: const/non-const 需要两个重载
    void greet() const { std::cout << "const\n"; }
    void greet()       { std::cout << "non-const\n"; }

    // C++23: 合并为单个模板函数
    template <typename Self>
    void greet(this Self&& self) {
        if constexpr (std::is_const_v<std::remove_reference_t<Self>>)
            std::cout << "const\n";
        else
            std::cout << "non-const\n";
    }
};
```

`this Self&& self` 是显式对象参数，编译器将对象作为第一个显式参数传递。

## 消除引用限定重载的重复

```cpp
// 传统 — 4 个重载，逻辑重复
struct Container {
    std::string& name() &              { return name_; }
    const std::string& name() const&   { return name_; }
    std::string name() &&             { return std::move(name_); }
};

// C++23 — 单个模板函数
struct Container {
    template <typename Self>
    auto&& name(this Self&& self) {
        return std::forward<Self>(self).name_;
    }
};
```

## CRTP 的替代方案

```cpp
// 传统 CRTP — 繁琐
template <typename Derived>
struct Base {
    void interface() { static_cast<Derived*>(this)->implementation(); }
};
struct MyClass : Base<MyClass> {
    void implementation() { /* ... */ }
};

// C++23 — 简洁
struct Base {
    template <typename Self>
    void interface(this Self&& self) { self.implementation(); }
};
struct MyClass : Base {
    void implementation() { /* ... */ }
};
```

### 实际应用：多态拷贝

```cpp
struct Shape {
    virtual ~Shape() = default;
    template <typename Self>
    auto clone(this Self&& self) {
        return std::make_unique<std::decay_t<Self>>(std::forward<Self>(self));
    }
};

struct Circle : Shape {
    double radius;
    Circle(double r) : radius(r) {}
};

auto c = std::make_unique<Circle>(5.0);
auto c2 = c->clone();  // unique_ptr<Circle>
```

## 递归 Lambda

C++23 之前 lambda 无法直接递归调用自身。显式对象参数解决了这个问题：

```cpp
// C++23 递归 lambda — 零额外开销
auto factorial = [](this auto self, int n) -> int {
    return n <= 1 ? 1 : n * self(n - 1);
};
std::cout << factorial(5) << "\n";  // 120

// 递归遍历树
auto traverse = [](this auto self, const Tree* node) -> void {
    if (!node) return;
    self(node->left);
    std::cout << node->value << " ";
    self(node->right);
};
```

传统方式需要 `std::function`（有间接调用开销）或 Y 组合子（复杂）。C++23 的方式编译器直接内联递归调用。

## 值类别推导

```cpp
struct StringWrapper {
    std::string data;
    template <typename Self>
    auto&& get(this Self&& self) {
        return std::forward<Self>(self).data;
    }
};

StringWrapper w{"hello"};
auto& s = w.get();                  // string&
auto&& s2 = std::move(w).get();    // string&&
```

## 缓存友好的 memoization

```cpp
auto fib_memo = [](this auto self, int n,
                   std::unordered_map<int, int>& cache) -> int {
    if (n <= 1) return n;
    if (auto it = cache.find(n); it != cache.end()) return it->second;
    int result = self(n - 1, cache) + self(n - 2, cache);
    return cache[n] = result;
};
```

## 注意事项

- 显式对象参数不能与传统隐式 `this` 共存于同一成员函数
- 不能用于构造函数、析构函数（`operator()` 在 lambda 中可以）
- 虚函数可以使用，但 `Self` 推导的是静态类型
- `this` 参数必须是函数的第一个参数
