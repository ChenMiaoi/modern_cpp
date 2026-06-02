---
title: 名称查找（Name Lookup）
topic: topics
feature: name-lookup
status_checked_at: 2026-06-02
standard: N/A
---

# 名称查找（Name Lookup）

## 概述

C++ 的名称查找（name lookup）是编译器将一个名字解析为具体声明的过程。它是重载决议、模板实例化等后续步骤的前提。标准将查找分为两大类：**非限定查找**（unqualified lookup）和**限定查找**（qualified lookup），再叠加 ADL、模板两阶段查找等特殊规则。理解这些规则是读懂编译器报错、避免隐蔽 bug 的基础。

```
名称查找流程：
  1. 名字是限定名（含 ::）？ → 限定查找（仅搜索指定作用域）
  2. 非限定名 → 非限定查找（逐层向外搜索）
  3. 如果调用表达式有函数参数 → 额外进行 ADL
  4. 如果在模板定义中 → 两阶段查找，非依赖名立即查找，依赖名推迟到实例化
  5. 找到候选集后 → 进行重载决议
```

## 非限定查找（Unqualified Lookup）

非限定查找从使用点所在的最内层作用域开始，逐层向外搜索，直到找到至少一个匹配项或到达全局作用域。

```cpp
int x = 10;

namespace A {
    int x = 20;
    namespace B {
        int x = 30;
        void f() {
            int x = 40;
            // 查找顺序：f 局部 → B → A → 全局
            // 找到 x = 40，查找停止
            int val = x; // 40
        }
    }
}
```

**关键规则——找到即停**：一旦在某一层找到匹配的名字，查找停止，外层同名声明被**隐藏**（hidden），不会进行重载：

```cpp
void f(int);        // 全局

namespace N {
    void f(double);  // N::f 隐藏了 ::f
    void g() {
        f(42);       // 找到 N::f(double)，::f(int) 不在候选集中
                     // 42 → double，调用 N::f(double)
    }
}
```

**类作用域中的查找**：类成员函数体内的名称查找先搜索类作用域（包括基类），再搜索外围命名空间：

```cpp
namespace N {
    int value = 1;
}

struct Base {
    int value = 2;
};

struct Derived : Base {
    void f() {
        int value = 3;      // 局部变量
        int a = value;       // 3 — 局部变量
        int b = this->value; // 2 — 通过 this 查找基类成员
        int c = N::value;    // 1 — 限定查找
    }
};
```

## 限定查找（Qualified Lookup）

限定查找仅在指定的作用域中搜索，**不向外层扩展**，也**不进行 ADL**：

```cpp
namespace A {
    void f(int);
    namespace B {
        void f(double);
        void g() {
            A::f(42);   // 只在 A 中查找 → A::f(int)
            f(42);       // 非限定查找 → B::f(double)（隐藏 A::f）
        }
    }
}
```

限定查找的关键特征：

```cpp
// 1. 不做 ADL
namespace N {
    struct S {};
    void serialize(S);
}

N::S s;
N::serialize(s);    // ✅ 限定查找找到 N::serialize
// N::serialize 在限定查找中不会触发 ADL，但这里恰好在 N 中找到了

// 2. 全局作用域前缀
::printf("hello");  // 只在全局作用域查找，跳过所有命名空间

// 3. 查找类成员时，搜索基类
struct Base { static int x; };
struct Derived : Base {
    void g() {
        Derived::x;   // 限定查找沿继承链找到 Base::x
    }
};
```

## ADL（Argument-Dependent Lookup / Koenig Lookup）

ADL 是**非限定查找的补充规则**：当函数调用的实参属于某个命名空间（或类）时，编译器还会在该命名空间（或类关联的命名空间）中搜索函数名。

```cpp
namespace StringLib {
    struct String {};
    void print(const String&);  // 声明在此命名空间
}

StringLib::String s;
print(s);  // 非限定查找不到全局 print → ADL 在 StringLib 中找到 print
```

**ADL 查找的"关联命名空间"**（associated namespaces）：

```
实参类型 T 的关联命名空间包括：
  1. T 本身是类 → T 所在的命名空间
  2. T 的所有基类所在的命名空间
  3. T 是模板实例 → 模板参数类型的关联命名空间
  4. T 是枚举 → 枚举所在的命名空间
  5. T 是指针/数组 → 指向/元素类型的关联命名空间
```

```cpp
namespace A {
    struct Base {};
    void process(Base);
}

namespace B {
    struct Derived : A::Base {};
    void process(Derived);
}

B::Derived d;
process(d);  // ADL 搜索 A 和 B（A 是 Base 的关联命名空间，B 是 Derived 的）
             // 找到 A::process(Base) 和 B::process(Derived)
             // 重载决议选择 B::process(Derived)（更精确匹配）
```

**ADL 不在以下情况中触发**：

```cpp
// 1. 限定调用
std::swap(a, b);    // 限定查找，不做 ADL（但 std::swap 是找到的候选）

// 2. 函数指针
auto fp = &process; // 不做 ADL

// 3. 声明中
using std::swap;    // 不做 ADL
```

## ADL 与 operator<<（最经典的 ADL 场景）

为什么 `std::cout << obj` 能找到 `operator<<(std::ostream&, const MyType&)` 即使它定义在 `MyNamespace` 中？ADL。

```cpp
#include <iostream>

namespace MyLib {
    struct Point { int x, y; };

    // operator<< 定义在 MyLib 中，而非 std 中
    std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << '(' << p.x << ',' << p.y << ')';
    }
}

int main() {
    MyLib::Point p{1, 2};
    std::cout << p << '\n';
    // operator<<(std::cout, p)
    //   → std::cout 的类型是 std::ostream
    //   → p 的类型是 MyLib::Point
    //   → ADL 搜索 std 和 MyLib
    //   → 在 MyLib 中找到 operator<<(ostream&, const Point&)
}
```

**这就是为什么 `operator<<` 必须定义在类型所在的命名空间中（或 `std` 中）**——否则 ADL 找不到它。

```cpp
// ❌ 错误：定义在全局作用域，不在 MyLib 中
namespace MyLib { struct Foo {}; }
std::ostream& operator<<(std::ostream& os, const MyLib::Foo&) { return os; }

// 如果 Foo 在 std 命名空间也没有关联，ADL 只搜索 std 和 MyLib，
// 不搜索全局作用域 → 编译失败

// ✅ 正确：定义在 MyLib 中
namespace MyLib {
    struct Bar {};
    std::ostream& operator<<(std::ostream& os, const Bar&) { return os; }
}
```

## Friend 声明与名称注入

Friend 声明可以将一个函数引入类所在的命名空间，但有一个关键限制：**只有当 friend 声明是函数的唯一声明时，才会真正注入**。

```cpp
namespace N {
    struct S {
        friend void hidden_friend(S);  // 声明在 N 中，但仅通过 ADL 可见
    };
}

N::S s;
hidden_friend(s);    // ✅ ADL 在 N 中找到
// hidden_friend(s); // 如果先做非限定查找（不带 S 类型的参数），找不到
```

**Friend 定义在类体内**（隐藏友元 / hidden friend）——这是现代 C++ 的常见模式：

```cpp
namespace util {
    template <typename T>
    struct Wrapper {
        T value;
        // 友元函数定义在类体内，仅通过 ADL 可见
        // 不会污染外围命名空间
        friend bool operator==(const Wrapper& a, const Wrapper& b) {
            return a.value == b.value;
        }
    };
}

util::Wrapper<int> a{1}, b{2};
a == b;  // ✅ ADL 找到 operator==（实参类型是 util::Wrapper<int>）
// operator==(a, b); // ❌ 非限定查找（无 ADL）找不到
```

**与已有声明交互**：

```cpp
void g(int);          // 全局声明

struct S {
    friend void g(int);  // 不是新声明，只是让 g 可以访问 S 的私有成员
};

// 如果 friend 声明是一个全新函数的唯一声明：
struct T {
    friend void h(T);    // 注入到外围命名空间（但仅 ADL 可见）
};
```

## 继承中的名称隐藏（Name Hiding）

派生类中的同名成员（无论签名是否匹配）会**隐藏**基类中的同名成员。这是非限定查找"找到即停"规则的直接结果。

```cpp
struct Base {
    void f(int);
    void f(double);
    void g(int);
};

struct Derived : Base {
    void f(double);  // 隐藏了 Base::f(int) 和 Base::f(double)
    // 使用 using 声明恢复
    using Base::f;   // 现在 Base::f(int) 和 Base::f(double) 都可见
};

void test(Derived& d) {
    d.f(42);         // 42 → double，调用 Derived::f(double)
    // 没有 using 的话，Base::f(int) 被隐藏，不会参与重载
}
```

**数据成员同样隐藏**：

```cpp
struct Base { int x = 1; };
struct Derived : Base { int x = 2; };

Derived d;
d.x;            // 2 — Derived::x 隐藏了 Base::x
d.Base::x;      // 1 — 限定查找访问基类成员
```

**为什么 C++ 要这样设计**：不同作用域的同名函数不应隐式参与重载——派生类的作者可能不知道基类有哪些重载，隐式重载会导致不可预测的函数调用。

## using 声明与 using 指令

### using 声明（using-declaration）

using 声明将一个**具体的名字**引入当前作用域。它创建的是一个别名（alias），不是副本：

```cpp
namespace A {
    void f(int);
    void f(double);
}

void g() {
    using A::f;     // 将 A::f 引入 g 的作用域
    f(42);          // A::f(int)
    f(3.14);        // A::f(double)
}

// 在类中使用 using 声明恢复被隐藏的基类成员
struct Derived : Base {
    using Base::f;  // 将 Base::f 引入 Derived 作用域，与 Derived::f 共存参与重载
};
```

### using 指令（using-directive）

using 指令将一个命名空间中的**所有名字**提升到包含该 using 指令和命名空间声明的**共同外围作用域**：

```cpp
namespace A {
    int x = 1;
    void f(int);
}

namespace B {
    int x = 2;
    void f(double);
}

void g() {
    using namespace A;
    using namespace B;
    // x 被提升到全局作用域，A::x 和 B::x 都是候选 → 二义性
    // int val = x;  // ❌ 错误：二义性

    // 但 f 可以通过重载决议区分
    f(42);     // A::f(int)
    f(3.14);   // B::f(double)
}
```

**using 指令的本质**：名字被注入到一个"影子作用域"（transitive scope），该作用域包含 using 指令所在作用域和命名空间声明所在作用域的共同祖先。这意味着 using 指令不会把名字直接注入到当前作用域，而是注入到一个更高的层级。

```cpp
namespace A { int v = 1; }

void f() {
    {
        using namespace A;  // v 被提升到 f 的作用域（不是内层块）
        v;                  // ✅ 找到 A::v
    }
    // v;  // ❌ using 指令的效果在块结束时消失（C++17 之前可能不同）
}
```

## 命名空间别名（Namespace Aliasing）

命名空间别名是纯粹的语法糖，不影响名称查找语义：

```cpp
namespace VeryLongNamespaceName {
    struct Widget {};
    void process(Widget);
}

namespace VLN = VeryLongNamespaceName;  // 别名

VLN::Widget w;      // 等价于 VeryLongNamespaceName::Widget w;
VLN::process(w);    // 等价于 VeryLongNamespaceName::process(w);

// ADL 基于原始命名空间，不是别名
// process(w) 的 ADL 搜索 VeryLongNamespaceName，不是 VLN
```

内联命名空间（inline namespace）别名更值得注意：

```cpp
namespace v2 {
    inline namespace v2_0 {  // 内联命名空间
        struct Config {};
    }
}

v2::Config c1;       // OK — v2_0 是内联的，v2::Config 即 v2::v2_0::Config
v2::v2_0::Config c2; // OK — 显式指定

// ADL：v2::Config 的关联命名空间是 v2 和 v2::v2_0
```

## 模板两阶段查找（Two-Phase Name Lookup）

C++ 模板标准要求**两阶段查找**：模板定义时查找非依赖名，实例化时查找依赖名。

```cpp
void process(int) { std::cout << "int\n"; }

template <typename T>
void foo(T val) {
    process(42);     // 非依赖名 — 第一阶段（定义时）立即查找
    process(val);    // 依赖名 — 第二阶段（实例化时）查找
}

void process(double) { std::cout << "double\n"; }

foo(3.14);
// process(42)   → 查找发生在 foo 定义之前，只找到 ::process(int) → 输出 "int"
// process(val)  → 依赖名，实例化时查找，ADL 搜索 → 找到两个 process，重载决议选 double
```

**MSVC 的历史问题**：MSVC 长期默认不实现两阶段查找（只做一次查找），`/permissive-` 标志可启用标准行为。这是跨编译器可移植性的常见陷阱。

## 依赖名与非依赖名（Dependent vs Non-Dependent Names）

在模板中，名称是否依赖于模板参数决定了其查找时机：

```cpp
template <typename T>
struct Traits {
    using type = typename T::value_type;  // 依赖名 — T 未确定时不查找
};

// 非依赖名的例子
extern int global_val;

template <typename T>
void f() {
    int x = global_val;  // 非依赖名 — 立即查找，绑定到当前声明
}
```

**判断规则**：

```
依赖名（dependent name）：
  - 形如 T::name（T 是模板参数）
  - 形如 expr.name，其中 expr 的类型依赖于模板参数
  - 形如 expr->name，同上
  - 形如 func(args...)，至少一个实参类型依赖于模板参数
  - 形如 T(args...)、T{args...}

非依赖名（non-dependent name）：
  - 所有其他情况
  - 查找发生在模板定义时（第一阶段）
  - 绑定到定义时可见的声明，后续添加的声明不影响
```

```cpp
// ⚠️ 经典陷阱
void helper(int) { std::cout << "int\n"; }

template <typename T>
void g(T val) {
    helper(val);     // 依赖名（val 的类型依赖 T）
    helper(1.0);     // 非依赖名（实参类型是 double，不依赖 T）
}

void helper(double) { std::cout << "double\n"; }

g(42);
// helper(val)  → 依赖名，实例化时 ADL+普通查找 → helper(int)（全局可见）
// helper(1.0)  → 非依赖名，定义时查找 → 只找到 helper(int)
//                 → 1.0 隐式转换为 int → 输出 "int"（不是 double！）
```

## typename 关键字的必要性

在模板中，依赖于模板参数的嵌套类型名前面必须加 `typename`，否则编译器默认将其解析为**非类型**（变量、静态成员等）：

```cpp
template <typename T>
void f() {
    // T::type 可能是类型，也可能是静态成员变量
    // 编译器默认假定它不是类型
    // T::type* p;          // ❌ 错误：T::type 被解析为值，* p 是乘法

    typename T::type* p;    // ✅ 明确告知编译器 T::type 是类型

    // C++20 改善：在某些上下文中 typename 可省略
    // 函数返回类型、参数类型（C++20 起）
}
```

**C++20 简化**（P0634R3）：在以下上下文中可省略 `typename`：
- 函数声明的返回类型
- 函数参数类型
- 模板参数（作为类型模板参数的实参）
- 数据成员声明的类型说明符
- 变量声明的类型说明符（`auto` 以外的）

```cpp
template <typename T>
// C++11: typename T::iterator foo();  // 必须有 typename
// C++20:
T::iterator foo();  // ✅ 省略 typename

template <typename T>
struct S {
    T::value_type data;  // ✅ C++20 省略 typename
};
```

## template 关键字用于依赖模板名

当一个依赖名是一个模板时，必须用 `template` 关键字告知编译器后面的 `<` 是模板参数列表的开始，而非比较运算符：

```cpp
template <typename T>
void f() {
    // T::create<int>(42);        // ❌ 编译器将 < 解析为比较运算符
    T::template create<int>(42);  // ✅ 明确告知 create 是模板

    // 同理适用于成员模板
    T obj;
    obj.template get<int>();       // ✅
    // obj.get<int>();             // ❌ 在某些编译器上可能失败
}
```

**组合使用 `typename` 和 `template`**：

```cpp
template <typename T>
void g() {
    // 获取一个嵌套模板的实例化类型
    typename T::template Container<int>::value_type val{};
    //  ↑ typename: Container<int>::value_type 是类型
    //                    ↑ template: Container 是模板
}
```

## 依赖名查找顺序

对于模板中的依赖名，标准定义了特殊的查找顺序：

```
依赖名查找顺序：
  1. 模板定义时可见的普通查找（两阶段查找的第一阶段结果）
  2. 模板实例化时的 ADL（基于实参的关联命名空间）

重要：依赖名不做"实例化时的普通查找"
```

```cpp
namespace N {
    struct S {};
    void f(S);           // ① 定义时可见
}

template <typename T>
void g(T val) {
    f(val);              // 依赖名：查找 ① 定义时可见的 f + ② 实例化时 ADL
}

// 之后添加
void f(int);            // ③ 不存在于定义时的查找结果中

N::S s;
g(s);                   // ① 找到 N::f(S)（定义时可见）；② ADL 也找到 N::f(S)
g(42);                  // ① 找到 N::f(S)？不，S 和 int 不同
                        //   ① 无匹配 → ② ADL 搜索 int 的关联命名空间（无）→ 失败
```

这个查找顺序解释了为什么依赖名的查找结果与非依赖名不同：非依赖名只使用定义时的普通查找，而依赖名额外加上实例化时的 ADL。

## CRTP 与名称查找陷阱

CRTP（Curiously Recurring Template Pattern）中，基类模板访问派生类成员时，名称查找的时机是关键问题：

```cpp
template <typename Derived>
struct Base {
    void interface() {
        // this->impl();        // ✅ 依赖名（this 类型依赖 Derived）
        // impl();              // ❌ 非依赖名，第一阶段查找，找不到 Derived::impl
        static_cast<Derived*>(this)->impl();  // ✅ 但繁琐
    }

    // C++ 最佳实践：用 this-> 访问派生类成员
    void better() {
        this->impl();  // ✅ this 的类型是 Base<Derived>* → 依赖名
    }
};

struct MyDerived : Base<MyDerived> {
    void impl() { std::cout << "MyDerived\n"; }
};
```

**`this->` 是让基类成员名变成依赖名的标准技巧**。不加 `this->` 的话，名字在第一阶段查找，此时派生类尚未定义，查找失败。

```cpp
// 另一个 CRTP 陷阱：静态成员
template <typename D>
struct Counter {
    static int count;
    void increment() {
        // count++;            // ❌ 非依赖名，可能绑定到外层的 count
        Counter::count++;      // ✅ 限定查找，不依赖模板参数
        // 或
        this->count++;         // ✅ 依赖名
    }
};
```

## P0846：ADL 与函数模板（C++20）

C++20 之前，ADL 只搜索关联命名空间中**可见的函数声明**，不搜索**函数模板**的特化——如果一个函数模板尚未实例化，它的特化在 ADL 中不可见。

P0846R0（C++20）放宽了限制：ADL 现在会搜索关联命名空间中的函数模板，即使尚未实例化。

```cpp
namespace N {
    template <typename T>
    void serialize(T);  // 函数模板声明

    struct Widget {};
    // serialize<Widget> 从未显式实例化
}

N::Widget w;
serialize(w);  // C++17: ADL 找到 N::serialize 模板，但其特化未实例化 → 可能失败
               // C++20（P0846）: ADL 找到 N::serialize 模板 → 实例化 serialize<Widget> → OK
```

这个改变解决了以下模式的可移植性问题：

```cpp
namespace ext {
    template <typename T>
    void to_string(T const&);  // 前向声明

    struct Error {};
    // 未定义 to_string<Error>，但声明存在
}

ext::Error e;
to_string(e);  // C++17：行为可能因编译器而异
               // C++20：标准要求 ADL 找到这个模板声明
```

## 常见名称查找陷阱

### 1. std 命名空间中的 ADL

```cpp
// ❌ 调用 swap 时限定为 std::swap 会阻止 ADL
// 正确做法：using std::swap; 然后调用 swap(a, b)
template <typename T>
void bad_swap(T& a, T& b) {
    std::swap(a, b);  // 只搜索 std，不搜索 T 的关联命名空间
}

template <typename T>
void good_swap(T& a, T& b) {
    using std::swap;    // 将 std::swap 引入当前作用域
    swap(a, b);         // 非限定查找 + ADL → 自定义 swap 优先于 std::swap
}
```

### 2. using 指令导致二义性

```cpp
namespace A { void f(int); }
namespace B { void f(int); }

void g() {
    using namespace A;
    using namespace B;
    // f(42);  // ❌ 二义性：A::f(int) 和 B::f(int)
}
```

### 3. 实参依赖查找与 ADL 不匹配

```cpp
namespace N {
    struct X {};
    void process(X);
}

N::X x;
// process(x);  // OK — ADL 找到 N::process
// 但如果同时有一个全局 process：
void process(int);
// process(x);  // 仍然 OK — ADL 找到 N::process，全局 process(X) 不匹配
// process(42); // OK — 非限定查找找到全局 process(int)
```

### 4. 模板中忘记 typename

```cpp
template <typename Container>
void print_first(Container& c) {
    // Container::iterator it = c.begin();   // ❌ 编译错误
    typename Container::iterator it = c.begin();  // ✅
}
```

### 5. 模板中忘记 template

```cpp
template <typename Allocator>
void rebind_example(Allocator& alloc) {
    // Allocator::rebind<int>::other int_alloc(alloc);           // ❌
    typename Allocator::template rebind<int>::other int_alloc(alloc);  // ✅
}
```

### 6. 继承模板基类不加 this->

```cpp
template <typename T>
struct Base {
    T data;
    void set(T val) { data = val; }
};

template <typename T>
struct Derived : Base<T> {
    void foo(T val) {
        // set(val);              // ❌ 非依赖名，找不到 Base<T>::set
        // data = val;            // ❌ 同理
        this->set(val);          // ✅ 依赖名
        this->data = val;        // ✅ 依赖名
        Base<T>::set(val);       // ✅ 限定查找（但会抑制虚函数动态分派）
    }
};
```

### 7. using 声明与重载的交互

```cpp
struct Base {
    void f(int);
};

struct Derived : Base {
    using Base::f;    // 引入 Base::f(int)
    void f(double);   // 与 Base::f(int) 共存
};

Derived d;
d.f(42);    // int → 精确匹配 Base::f(int)
d.f(3.14);  // double → 精确匹配 Derived::f(double)
```

### 8. ADL 与隐式转换不配合

```cpp
namespace N {
    struct S { S(int); };  // 隐式转换从 int
    void foo(S);
}

foo(42);  // ❌ ADL 不考虑隐式转换
          // int 的关联命名空间中没有 foo
          // 必须先转换为 N::S 才能触发 ADL
foo(N::S{42});  // ✅ 显式构造，ADL 在 N 中找到 foo
```

## 延伸阅读

- [模板实例化](/topics/template-instantiation) — 显式/隐式实例化、特化与名称查找的交互
- [重载决议](/topics/overload-resolution) — 名称查找之后的候选函数选择
- [值类别](/topics/value-categories) — 左值/右值如何影响函数参数绑定
- [RAII 与资源管理](/topics/raii) — 资源管理模式中名称查找的实际应用
- C++ 标准 [basic.lookup](https://eel.is/c++draft/basic.lookup) — 名称查找的形式化规则
- cppreference [Name lookup](https://en.cppreference.com/w/cpp/language/lookup) — 完整参考
