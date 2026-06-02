---
title: "C++98 语言特性"
topic: unknown
feature: features
standard: N/A
status_checked_at: 2026-06-02
---
# C++98 语言特性

## 类与面向对象

C++98 在 C 的基础上引入了完整的面向对象体系：

- **类定义**：封装数据和行为
- **继承**：单继承、多继承、虚继承
- **多态**：虚函数（`virtual`）、纯虚函数
- **访问控制**：`public`、`protected`、`private`
- **抽象基类**：含纯虚函数的类不可实例化

## 模板

函数模板和类模板是泛型编程的基石：

```cpp
// 函数模板
template<typename T>
T max(T a, T b) { return a > b ? a : b; }

// 类模板
template<typename T>
class Stack {
    std::vector<T> elems;
public:
    void push(const T& elem);
    T pop();
};
```

## 异常处理

```cpp
try {
    // 可能抛出异常的代码
    throw std::runtime_error("something went wrong");
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
} catch (...) {
    // 捕获所有异常
}
```

## 命名空间

```cpp
namespace MyLib {
    class Widget { /* ... */ };
}

MyLib::Widget w;  // 使用时通过命名空间限定
```

## 其他特性

- **RTTI**：`typeid`、`dynamic_cast`
- **`const` 修饰符**：变量、指针、引用、成员函数
- **引用**：左值引用（`T&`）
- **运算符重载**
- **`new` / `delete`**
- **`static_cast` / `dynamic_cast` / `const_cast` / `reinterpret_cast`**
