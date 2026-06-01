# nullptr — 类型安全的空指针字面量

## 概述

在 C++11 之前，表示空指针有 `0` 和 `NULL` 两种方式，二者都会带来歧义——编译器无法从语法层面区分"整数零"和"空指针"。C++11 引入 `nullptr` 关键字，类型为 `std::nullptr_t`，彻底消除了空指针与整数之间的语义混淆。

## 为什么需要 nullptr

传统 C++ 中 `NULL` 通常被定义为 `0`，它同时满足整数和指针两个角色：

```cpp
void f(int);
void f(char*);

f(0);    // calls f(int) — 0 is an integer literal
f(NULL); // calls f(int) on most compilers — NULL expands to 0
```

程序员的意图是调用 `f(char*)`，但 `NULL` 展开后就是 `0`，编译器匹配到 `f(int)`。模板场景中问题更严重：

```cpp
template <typename T>
void process(T value) { /* ... */ }

process(0);       // T = int — probably not what you want
process(NULL);    // T = int — same problem
process(nullptr); // T = std::nullptr_t — correct
```

`T` 被推导为 `int` 后，后续基于指针的操作会产生编译错误或未定义行为，且错误信息极其晦涩。

## `std::nullptr_t` 类型

`nullptr` 的类型是 `std::nullptr_t`（定义在 `<cstddef>` 中），关键属性：

1. 可隐式转换为任意指针类型（包括函数指针和成员指针）
2. **不可**隐式转换为整数类型
3. **不可**进行算术运算

```cpp
#include <cstddef>
#include <type_traits>

static_assert(sizeof(nullptr) == sizeof(void*), "pointer-sized");
static_assert(std::is_same<decltype(nullptr), std::nullptr_t>::value, "");

int*    p1 = nullptr;   // OK
void (*fp)() = nullptr; // OK — function pointer
int S::* mp = nullptr;  // OK — member pointer
// int x = nullptr;     // error: conversion from nullptr_t to non-scalar type
```

## 重载决议中的行为

```cpp
void handle(int value) { /* int overload */ }
void handle(int* ptr)  { /* pointer overload */ }

handle(0);       // calls handle(int)
handle(NULL);    // calls handle(int) on most platforms
handle(nullptr); // calls handle(int*) — unambiguously
```

`nullptr` 明确表达"空指针"语义，编译器不会将其误判为整数。

## 与模板的交互

### 智能指针场景

```cpp
template <typename T>
class SmartPtr {
public:
    SmartPtr(T* p) : ptr_(p) {}
    bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
private:
    T* ptr_;
};

SmartPtr<int> sp(nullptr);
if (sp == nullptr) { /* compiles cleanly */ }
```

### 无法推导指针所指类型

`nullptr` 本身没有指向类型，不能直接用于需要推导 `T*` 的场景：

```cpp
template <typename T>
void takes_ptr(T* p) { /* ... */ }

takes_ptr(nullptr);                  // error: cannot deduce T from nullptr_t
takes_ptr(static_cast<int*>(nullptr)); // OK — explicit
```

## 常见陷阱

### 陷阱 1：与 bool 重载的歧义

```cpp
void f(int*);
void f(bool);

f(nullptr);  // C++11/14: ambiguous — nullptr converts to both int* and bool
```

遇到此情况需显式转换或重新设计接口。

### 陷阱 2：布尔上下文中使用 nullptr

```cpp
int* p = get_pointer();
if (p)           { /* OK — idiomatic */ }
if (p != nullptr) { /* OK — more explicit, preferred in modern C++ */ }
```

两种写法都正确，但 `!= nullptr` 在 code review 中意图更清晰。

### 陷阱 3：C 风格 API 边界

```cpp
extern "C" void c_function(void* p);
c_function(nullptr); // OK — nullptr converts to void*
```

在所有主流 ABI 上，`nullptr` 的表示与 C 的空指针一致，但标准不保证。

## 从 NULL / 0 迁移指南

**步骤 1：全局替换 NULL** — 将所有表示空指针的 `NULL` 替换为 `nullptr`（保留宏定义中的）。

**步骤 2：替换字面量 0** — `0` 有时确实是整数零，需人工判断：

```cpp
int* p = 0;              // → int* p = nullptr;
if (p == 0) { }          // → if (p == nullptr) { }
return 0;                // in pointer-returning ctx → return nullptr;
```

**步骤 3：编译器警告辅助迁移**：

```bash
clang++ -Wzero-as-null-pointer-constant -std=c++11 source.cpp
g++     -Wzero-as-null-pointer-constant -std=c++11 source.cpp
```

此警告在所有将 `0`/`NULL` 用作空指针的位置报告警告，是迁移的有力工具。

## 最佳实践

1. 始终使用 `nullptr` 表示空指针，不使用 `0` 或 `NULL`。
2. 模板代码中，`nullptr` 是唯一正确的空指针表示——它不会污染类型推导。
3. 函数参数语义为"可选指针"时，使用 `nullptr` 而非默认参数 `= 0`。
4. 开启 `-Wzero-as-null-pointer-constant` 编译器警告，在 CI 中强制执行。
5. 不要将 `nullptr` 与 `bool` 重载混用——如果接口同时接受指针和布尔值，重新设计 API。
6. C 风格 API 中仍可使用 `NULL`，但 C++ 侧应立即转换为 `nullptr`。
