# C++17 类模板参数推导（CTAD）

## 概述

C++17 引入了 **类模板参数推导（Class Template Argument Deduction, CTAD）**，允许在构造类模板对象时省略模板参数，编译器从构造函数参数中自动推导。这消除了对 `make_xxx` 工厂函数的依赖，简化了代码。

## 语法

```cpp
// C++17 之前：必须显式指定模板参数
std::pair<int, double> p1(1, 3.14);
auto p2 = std::make_pair(1, 3.14);

// C++17：直接推导
std::pair p3(1, 3.14);          // pair<int, double>
std::vector v{1, 2, 3};         // vector<int>
```

## 推导规则

### 隐式推导指南

编译器从每个构造函数自动生成隐式推导指南：

```cpp
template <typename T>
struct Wrapper {
    T value;
    Wrapper(T v) : value(v) {}
};
// 隐式生成：template <typename T> Wrapper(T) -> Wrapper<T>;

Wrapper w(42);       // T = int
Wrapper w2("hello"); // T = const char*
```

### 显式推导指南

```cpp
template <typename T>
struct Box { T content; };

Box(const char*) -> Box<std::string>;  // 显式指南

Box b("hello");   // Box<std::string>，而非 Box<const char*>
Box b2(42);       // Box<int>
```

## 替代 make_pair / make_tuple

```cpp
// C++14
auto p = std::make_pair(1, "hello");

// C++17 CTAD
std::pair p(1, "hello");
std::tuple t(1, 3.14, "x");
```

`make_xxx` 仍可用于需要完美转发或 `decay` 的场景。

## std::array CTAD

```cpp
// C++14：繁琐
std::array<int, 3> a1 = {1, 2, 3};

// C++17 CTAD
std::array a2 = {1, 2, 3};     // array<int, 3>
std::array a3 = {1.0, 2.0};   // array<double, 2>
```

## 用户自定义推导指南

### 从迭代器对推导

```cpp
template <typename T>
class SimpleVector {
    T* data_;
    std::size_t size_;
public:
    template <typename Iter>
    SimpleVector(Iter first, Iter last) { /* ... */ }
};

template <typename Iter>
SimpleVector(Iter, Iter)
    -> SimpleVector<typename std::iterator_traits<Iter>::value_type>;

std::vector<int> v = {1, 2, 3};
SimpleVector sv(v.begin(), v.end());  // SimpleVector<int>
```

### 多构造函数场景

```cpp
template <typename T>
struct Range {
    T begin_, end_, step_;
    Range(T begin, T end) : begin_(begin), end_(end), step_(1) {}
    Range(T begin, T end, T step) : begin_(begin), end_(end), step_(step) {}
};

Range r1(0, 10);           // Range<int>
Range r2(0.0, 10.0, 0.5); // Range<double>
// Range r3(0, 10.0);     // 错误：T 不能同时是 int 和 double
```

## 继承与 CTAD

```cpp
template <typename T>
struct Base {
    T value;
    Base(T v) : value(v) {}
};

template <typename T>
struct Derived : Base<T> {
    using Base<T>::Base;
};

Derived d(42);  // Derived<int>
```

## 限制

1. **聚合初始化有限**：C++17 对聚合类型 CTAD 支持不完整，C++20 才完善。
2. **别名模板不参与 CTAD**：
   ```cpp
   template <typename T> using Vec = std::vector<T>;
   // Vec v = {1, 2, 3};  // 错误
   ```
3. **推导指南必须与类定义在同一命名空间**。

## 最佳实践

1. **优先使用 CTAD 替代 `make_xxx`**。
2. **为自定义类提供显式推导指南**。
3. **注意隐式转换**：CTAD 推导精确类型，不做隐式转换。

## 常见陷阱

- **花括号 vs 圆括号**：`std::vector v1{3, 100}` 推导为 `vector<int>`（两个元素），与非 CTAD 行为一致但更易混淆。
- **别名模板不支持 CTAD**：必须使用原始模板名。
- **推导失败是硬错误**：不回退到其他构造函数。
- **隐式推导指南可能意外匹配**：模板构造函数生成的指南可能比预期更宽泛。
