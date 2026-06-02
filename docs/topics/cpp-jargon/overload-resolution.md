---
title: "重载决议与名字查找"
topic: unknown
feature: overload-resolution
standard: N/A
status_checked_at: 2026-06-02
---
# 重载决议与名字查找

## Overload Resolution（重载决议）

当多个函数同名时，编译器按以下步骤选择：

```
1. 名字查找（Name Lookup）——找到所有同名函数
2. 候选函数集（Candidate Functions）——参数数量匹配的
3. 可行函数集（Viable Functions）——参数类型可以转换的
4. 最佳匹配（Best Match）——按转换等级排序
```

### 转换等级（从好到差）

1. **精确匹配**（exact match）——类型完全相同
2. **提升**（promotion）——`short` → `int`、`float` → `double`
3. **标准转换**（conversion）——`int` → `double`、`Derived*` → `Base*`
4. **用户定义转换**（user-defined）——通过转换构造函数或转换运算符
5. **省略号**（ellipsis）——`...` 参数

```cpp
void f(int);       // #1
void f(double);    // #2
void f(...);       // #3

f(42);     // #1 精确匹配
f(3.14);   // #2 精确匹配
f('a');    // #1 通过 promotion（char → int）优于 #2 的 conversion
```

### 歧义

当两个重载一样好时，编译错误：

```cpp
void f(long);
void f(float);
f(42);  // 错误！int → long 和 int → float 一样好
```

## ADL（Argument-Dependent Lookup，参数依赖查找）

编译器不仅在调用点的作用域查找函数，还在参数类型所在的命名空间中查找：

```cpp
namespace MyLib {
  struct Widget {};
  void swap(Widget& a, Widget& b);  // MyLib::swap
}

MyLib::Widget a, b;
swap(a, b);  // 不需要写 MyLib::swap！
// 编译器在 MyLib 命名空间中找到了 swap
```

这就是为什么 `std::swap` 是通过 ADL 发现的——库不应该用自己的 swap 覆盖用户类型的 swap。

**ADL 的规则**：考虑每个参数类型的"关联命名空间"——包括类型的定义所在命名空间、模板参数的命名空间等。

## Two-Phase Lookup（两阶段查找）

模板中的名字查找分为两个阶段：

```
Phase 1（模板定义时）：查找非依赖名（不依赖模板参数的名字）
Phase 2（模板实例化时）：查找依赖名（依赖模板参数的名字）
```

```cpp
void f(int) { std::cout << "int version\n"; }

template<typename T>
void g(T x) {
  f(x);           // f 是依赖名（因为 x 依赖 T）
  // Phase 1: 不查找 f
  // Phase 2: 实例化时才查找
}

void f(double) { std::cout << "double version\n"; }

g(3.14);  // 调用 f(double)——Phase 2 查找时看到了 f(double)
```

MSVC 的传统行为：只做 Phase 2（推迟所有查找到实例化时）。`/permissive-` 标志启用两阶段查找。

## Dependent Name（依赖名）

依赖模板参数的名字需要 `typename` 消歧义：

```cpp
template<typename T>
void foo() {
  T::type* p;         // 错误！编译器认为 T::type * p 是乘法
  typename T::type* p; // 正确！告诉编译器 T::type 是一个类型
}
```

## Name Hiding（名字隐藏）

派生类中的名字会隐藏基类中同名的名字（即使签名不同）：

```cpp
struct Base {
  void f(int);
};

struct Derived : Base {
  void f(double);  // 隐藏了 Base::f(int)
};

Derived d;
d.f(42);     // 调用 Derived::f(double)！int 被隐式转换为 double
d.Base::f(42);  // 调用 Base::f(int)
```

使用 `using Base::f;` 可以解除隐藏。
