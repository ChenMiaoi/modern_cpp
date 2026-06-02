---
title: 值类别深入解析
topic: topics
feature: value-categories-deep-dive
status_checked_at: 2026-06-02
standard: C++17
---

# 值类别深入解析

## 历史演进：从 C 到 C++17

值类别（value category）的概念远早于 C++ 本身。理解它的演进历史有助于理解当前分类体系为何如此设计。

**C 语言时代**：表达式被简单地分为"左值"和"右值"。左值（lvalue）是能出现在赋值运算符**左**侧的表达式——本质上是有内存地址的对象。右值（rvalue）是一次性的值——字面量、算术运算结果。这个定义在 C 中清晰且自洽。

**C++98 的动摇**：C++ 引入了 `const`，使得"能出现在赋值左侧"不再是区分标准——`const int x = 42;` 中的 `x` 显然是左值，但 `x = 1` 无法通过编译。标准因此将 lvalue 重新定义为"有身份（identity）的表达式"——你可以取其地址（`&x` 合法）。

**C++11 的扩展**：移动语义的引入需要区分两种右值——一种是纯粹的值（字面量 `42`），另一种是"即将被移动"的值（`std::move(x)` 的结果）。后者虽然没有名字，但绑定到了一个有身份的对象上。标准因此引入了 xvalue（expiring value）的概念。

**C++17 的系统化**：临时对象实体化（temporary materialization）规则的确立使得值类别体系完成了最终定型。prvalue 不再是"临时对象"——它是一个**初始化器**（initializer），只有在需要时才被实体化为临时对象。

```
演进时间线：

C:     lvalue / rvalue                    （2 个类别）
C++98: lvalue / non-lvalue               （标准草案用语）
C++11: lvalue / xvalue / prvalue          （3 个基本类别 + 2 个组合）
C++17: 同 C++11，但 prvalue 语义彻底改变   （临时对象实体化）

组合类别：
  glvalue = lvalue + xvalue    （泛左值：有身份的表达式）
  rvalue  = xvalue + prvalue   （右值：可绑定到 T&& 的表达式）
```

## 完整分类体系

C++17 的值类别由两个正交属性的组合决定：

| 类别 | 有身份（has identity） | 可被移动（may be moved from） |
|------|:---:|:---:|
| lvalue | ✓ | ✗ |
| xvalue | ✓ | ✓ |
| prvalue | ✗ | ✓ |
| glvalue | ✓ | *（任意）* |
| rvalue | *（任意）* | ✓ |

**分类树**：

```
                        expression
                    ┌───────┴───────┐
                glvalue            rvalue
              ┌───┴───┐         ┌───┴───┐
          lvalue   xvalue    xvalue   prvalue
                                   ↑
                              两个集合的交集
```

判定规则的直觉理解：

- **lvalue**：你可以对它取地址（`&expr` 合法），它代表一个**持久对象**。典型例子：变量名、解引用表达式 `*p`、前置自增 `++i`、字符串字面量 `"hello"`、返回左值引用的函数调用。
- **prvalue**：它是一段纯粹的**计算结果**，没有独立的内存位置。典型例子：字面量 `42`、算术表达式 `a + b`、返回非引用类型的函数调用、lambda 表达式、`requires` 表达式。
- **xvalue**：它代表一个**即将结束生命周期的对象**，你可以从中"窃取"资源。典型例子：`std::move(x)` 的结果、返回右值引用的函数调用 `std::move(x)`、成员访问表达式 `prvalue.member`（C++17 实体化后）。

```cpp
// 判定练习：
int x = 42;
int& lr = x;              // x 是 lvalue（有名字、可取地址）
int&& rr = 42;            // 42 是 prvalue（纯字面量）
int&& mr = std::move(x);  // std::move(x) 是 xvalue

std::string s = "hi";
std::string&& t = std::move(s);   // std::move(s) 是 xvalue
std::string   u = std::move(s);   // 调用移动构造函数，xvalue 绑定到 &&
                                   // 注意：s 本身仍然是 lvalue，其值处于"合法但未指定"状态

// 非直觉的例子：
int a = 1, b = 2;
a + b;         // prvalue — 纯粹的计算结果
++a;           // lvalue — 前置 ++ 返回左值引用
a++;           // prvalue — 后置 ++ 返回旧值的拷贝

// 条件表达式：
true ? a : b;  // lvalue — 两个操作数都是 lvalue 时，结果是 lvalue
true ? a : 1;  // prvalue — 操作数不都是 lvalue 时，进行隐式转换
```

## 临时对象实体化（Materialization）—— C++17 的关键变革

C++17 引入了临时对象实体化转换（temporary materialization conversion），这是理解现代 C++ 值类别语义的**核心概念**。

**核心规则**：prvalue 不是对象——它是产生对象的一种**方式**（一种初始化器）。当需要将 prvalue 作为 glvalue 使用时（绑定引用、取成员、调用方法），编译器会进行临时对象实体化：创建一个临时对象，并用 prvalue 初始化它。转换的结果是一个 xvalue。

```cpp
struct Widget {
    int id;
    std::string name;
    Widget(int i, std::string n) : id(i), name(std::move(n)) {}
};

Widget make_widget() { return {1, "temp"}; }  // 返回 prvalue

// C++17 语义：
Widget w = make_widget();
// 1. make_widget() 是 prvalue——它不是一个对象，而是一种初始化 w 的方式
// 2. 直接在 w 的内存位置构造 Widget，零拷贝、零移动
// 这就是 guaranteed copy elision（保证拷贝消除）

// 需要实体化的场景：
Widget&& ref = make_widget();
// make_widget() 需要绑定到引用——引用需要一个对象来绑定
// → 触发临时对象实体化：在内存中创建临时 Widget（xvalue）
// → ref 绑定到该临时对象
// → 临时对象的生命周期延长到 ref 的生命周期

const Widget& cref = make_widget();
// 同上：需要实体化，然后 const 引用延长临时对象寿命

auto&& uref = make_widget();
// 同上：实体化 + 万能引用绑定

// 成员访问触发实体化：
int id = make_widget().id;
// make_widget() 是 prvalue，访问 .id 需要一个对象
// → 实体化为临时 Widget，然后取其 id 成员
// → 临时对象在完整表达式末尾销毁
```

实体化的具体时机：

```cpp
struct S { int x; };

S f() { return {42}; }

// 不触发实体化 —— prvalue 直接初始化目标对象
S s1 = f();

// 触发实体化 —— 绑定到引用
S&& s2 = f();

// 触发实体化 —— 访问成员
int v = f().x;

// 不触发实体化 —— 直接用于初始化（C++17 保证）
S s3 = S{S{f()}};   // 层层直接初始化，无临时对象
```

## 临时对象生命周期延长规则

临时对象通常在完整表达式（full-expression）末尾销毁，但 C++ 有两条特殊的延长规则：

**规则一：const 左值引用或右值引用绑定到临时对象时，临时对象的生命周期延长到引用的生命周期。**

```cpp
std::string make() { return "hello"; }

void example() {
    const std::string& r1 = make();       // 临时 string 的生命周期延长到 r1 的作用域
    std::string&& r2 = make();            // 同上

    // ⚠️ 陷阱：函数参数中的引用不享受延长
    // 因为临时对象的生命周期只延长到"绑定到的引用"的生命周期
    // 函数参数的生命周期 ≠ 函数内部引用的生命周期
}

void takes_ref(const std::string& s);    // s 的生命周期由调用者管理
void trap() {
    takes_ref(make());  // make() 的临时对象在 takes_ref 的调用表达式末尾销毁
    // 即使 takes_ref 内部保存了 const 引用，也已经悬空
}
```

**规则二：延长是有条件的——只在直接绑定时生效。**

```cpp
struct Holder {
    const std::string& ref;
    Holder(const std::string& r) : ref(r) {}  // ref 绑定到临时对象
};

void pitfall() {
    Holder h(make());   // make() 产生的临时 string 绑定到参数 r
                        // 参数 r 的生命周期在构造函数调用结束后结束
                        // 但 h.ref 指向的临时 string 已经销毁！
                        // h.ref 是悬空引用

    // 正确做法：
    std::string s = make();
    Holder h2(s);       // h2.ref 绑定到局部变量 s，安全
}
```

**延长的传播范围**：

```cpp
struct Aggregate {
    std::string s;
    int n;
};

void lifetime_extension_examples() {
    // 直接绑定——延长
    const Aggregate& a = Aggregate{42, "hi"};  // ✅ 延长
    const std::string& s = Aggregate{42, "hi"}.s; // ✅ 延长（子对象）

    // ⚠️ C++17 之前这里有个缺陷：绑定到子对象时延长的规则不一致
    // 但 C++17 的实体化规则统一了语义

    // 通过函数返回——不延长
    // （返回的临时对象在返回语句中创建，不是直接绑定）
    auto make_pair = []() -> std::pair<int, int> { return {1, 2}; };
    const auto& [a1, a2] = make_pair();  // ✅ 结构化绑定延长临时对象
    // 但注意：结构化绑定是否延长临时对象取决于实现方式
    // P0963（C++26 accepted）将标准化结构化绑定的生命周期延长行为
}
```

## 移动语义与值类别的交互

移动语义的核心依赖于值类别：只有 rvalue（prvalue 和 xvalue）能触发移动构造/赋值。`std::move` 的全部作用就是将 lvalue 转换为 xvalue——它不做任何实际的资源移动。

```cpp
// std::move 的实现（简化）
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}
// 它只做类型转换，不生成任何代码

// 重载决议中的值类别：
class Buffer {
    char* data_;
    std::size_t size_;
public:
    Buffer(const Buffer& o)        // 1. 拷贝构造——参数是 const lvalue&
        : data_(new char[o.size_]), size_(o.size_) {
        std::memcpy(data_, o.data_, size_);
    }
    Buffer(Buffer&& o) noexcept    // 2. 移动构造——参数是 rvalue&&
        : data_(o.data_), size_(o.size_) {
        o.data_ = nullptr;
        o.size_ = 0;
    }
};

Buffer a(100);
Buffer b(a);              // 调用 1——a 是 lvalue
Buffer c(std::move(a));   // 调用 2——std::move(a) 是 xvalue
Buffer d(Buffer(50));     // 调用 2——Buffer(50) 是 prvalue

// ⚠️ 移动后对象的状态
// std::move(a) 后，a 仍处于"合法但未指定"（valid but unspecified）状态
// 你可以对 a 做的唯一安全操作是：赋值、销毁
// 不要依赖 a 的具体值
```

**移动构造函数的值类别重载模式**：

```cpp
// 万能引用 + 转发——完美保持值类别
template <typename T>
void push_back(T&& value) {
    // T 是 U& 时，value 是 lvalue 引用→走拷贝路径
    // T 是 U  时，value 是 rvalue 引用→走移动路径
    // 但实际上容器的 push_back 通常是两个重载：
}

// 容器的典型实现（std::vector 的做法）
class Container {
    void push_back(const T& value);  // 拷贝版本——参数是 lvalue
    void push_back(T&& value);       // 移动版本——参数是 rvalue
};

// 对于派生类到基类的移动，需要注意切片问题：
struct Base {
    virtual ~Base() = default;
    Base() = default;
    Base(Base&&) = default;
};
struct Derived : Base {
    std::string data;
};

Derived d;
Base b1(d);             // 拷贝——切片，Derived 部分丢失
Base b2(std::move(d));  // 移动——仍然切片！移动的是 Base 子对象
// 正确做法：用指针/智能指针
std::unique_ptr<Base> b3 = std::make_unique<Derived>(std::move(d));
```

## 完美转发与值类别保持

`std::forward` 是保持值类别的核心工具。它与万能引用（forwarding reference）配合使用，确保值类别在传递过程中不丢失。

```cpp
// 问题：不使用 forward 时值类别丢失
template <typename T>
void wrapper_bad(T arg) {
    target(arg);  // arg 是 lvalue（有名字的参数始终是 lvalue）
                  // 即使调用者传入 rvalue，到 target 也变成了 lvalue
                  // → 总是走拷贝路径
}

// 正确：使用万能引用 + forward
template <typename T>
void wrapper_good(T&& arg) {
    target(std::forward<T>(arg));
    // T 是 U& 时（调用者传入 lvalue）：forward 返回 lvalue 引用
    // T 是 U  时（调用者传入 rvalue）：forward 返回 rvalue 引用
}
```

**`std::forward` 的内部机制**：

```cpp
// std::forward 的简化实现
template <typename T>
T&& forward(std::remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

// 为什么这样工作：
// 1. 当 T = int&（传入 lvalue 时的推导结果）：
//    签名变成 int& && forward(int&) → 引用折叠为 int& forward(int&)
//    返回 lvalue 引用 ✅
//
// 2. 当 T = int（传入 rvalue 时的推导结果）：
//    签名变成 int&& forward(int&)
//    返回 rvalue 引用 ✅
```

**转发的陷阱和限制**：

```cpp
// 陷阱 1：对同一参数多次 forward——未定义行为
template <typename T>
void bad(T&& arg) {
    target(std::forward<T>(arg));
    other(std::forward<T>(arg));  // ⚠️ UB：arg 可能已被移动
}

// 陷阱 2：大括号初始化列表不能被转发
template <typename T>
void wrapper(T&& arg) { target(std::forward<T>(arg)); }

// wrapper({1, 2, 3});  // ❌ 编译失败：T 无法推导
// 需要用 std::initializer_list 显式传递

// 陷阱 3：位域不能被转发
struct Flags {
    unsigned int mode : 3;
};
Flags f;
// wrapper(f.mode);  // ❌ 编译失败：位域不能绑定到引用

// 陷阱 4：NULL 指针和字符串字面量
template <typename T>
void func(T&& arg) { /* ... */ }
func(NULL);       // ⚠️ T 推导为 int（不是指针类型）
func(nullptr);    // ✅ T 推导为 std::nullptr_t
```

**参数转发包模式**：

```cpp
// 可变参数模板 + 完美转发——通用工厂模式
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    // 每个参数的值类别都被精确保持
}

// 完美转发 + 拷贝消除的交互
struct Widget {
    Widget(int, int) {}
    Widget(const Widget&) { std::puts("copy"); }
    Widget(Widget&&) { std::puts("move"); }
};

template <typename... Args>
Widget create(Args&&... args) {
    return Widget(std::forward<Args>(args)...);
}

// create(1, 2)           → 无输出：prvalue 直接初始化，拷贝消除
// Widget w; create(w)    → 输出 "copy"：lvalue 走拷贝
// create(std::move(w))   → 输出 "move"：xvalue 走移动
```

## decltype 与值类别

`decltype` 是唯一能精确捕获表达式值类别的语言机制。它的推导规则不同于 `auto`，理解这些规则对编写泛型代码至关重要。

**核心规则**：

```cpp
// 规则 1：decltype(实体) → 声明类型
int x = 42;
decltype(x) a = 10;          // int — x 的类型是 int

// 规则 2：decltype(表达式) → 表达式的值类别精确映射
int&  lr = x;
int&& rr = 42;
decltype(lr) b = x;           // int& — lvalue 表达式 → T&
decltype(rr) c = 42;          // int&& — lvalue 表达式 → T&&
                               // 注意：rr 是变量名，是 lvalue！即使其类型是 int&&

// 规则 3：decltype(prvalue) → T（非引用）
decltype(42) d = 0;           // int — 字面量是 prvalue → T
decltype(x + 0) e = 0;        // int — 算术结果是 prvalue → T

// 规则 4：decltype(lvalue of type T) → T&
int arr[3];
decltype(arr) f = {1, 2, 3};  // int(&)[3] — 数组名是 lvalue → 引用

// 规则 5：函数名
void foo();
decltype(foo) g = foo;        // void() — 函数名是 lvalue，但函数类型不加引用
```

**`decltype(auto)` 的威力**：

```cpp
// decltype(auto) 保留值类别——完美返回类型推导
template <typename Container, typename Index>
decltype(auto) access(Container&& c, Index i) {
    return std::forward<Container>(c)[i];
    // 如果 c[i] 返回 int&，则返回类型是 int&
    // 如果 c[i] 返回 int，则返回类型是 int
}

std::vector<int> v = {1, 2, 3};
decltype(auto) val = access(v, 0);  // val 是 int&

// 对比 auto：
auto x = v[0];        // x 是 int（拷贝）
auto& y = v[0];       // y 是 int&
auto&& z = v[0];      // z 是 int&（lvalue 绑定到 && 折叠为 &）

// 完美转发 lambda
auto wrapper = [](auto&& f, auto&&... args) -> decltype(auto) {
    return std::forward<decltype(f)>(f)(std::forward<decltype(args)>(args)...);
};
```

**decltype 的括号陷阱**：

```cpp
int x = 42;
decltype(x)   a;  // int   — 实体（变量名），直接取声明类型
decltype((x)) b;  // int&  — 表达式（(x) 是 lvalue 表达式），加引用
                   // 这是最常见的 decltype 陷阱

// 实际影响：
decltype((x)) c = x;  // c 是 int&，必须初始化
// decltype(x)  d;      // OK：int 可以默认初始化

// auto 不同——auto 总是剥离引用（除非用 auto& 或 auto&&）：
auto e = x;        // int
auto& f = x;       // int&
decltype(auto) g = (x);  // int&（decltype 规则：(x) 是 lvalue 表达式）
```

## 值类别之间的隐式转换

值类别之间存在特定的隐式转换路径，理解这些转换对理解重载决议和模板推导至关重要。

```
转换方向：

  lvalue ──────────────────────→ glvalue    (lvalue 本身即 glvalue，无需转换)
  xvalue ──────────────────────→ glvalue    (xvalue 本身即 glvalue，无需转换)
  xvalue ──────────────────────→ rvalue     (xvalue 本身即 rvalue，无需转换)
  prvalue ─────────────────────→ rvalue     (prvalue 本身即 rvalue，无需转换)

  lvalue ──── (绑定到 T&) ────→ lvalue     (引用绑定，不创建新对象)
  lvalue ──── (绑定到 const T&) ─→ lvalue  (const 引用绑定，可延长生命周期)
  prvalue ─── (实体化) ──────→ xvalue      (C++17：临时对象实体化转换)
  xvalue ──── (绑定到 T&&) ───→ rvalue     (右值引用绑定)

类型系统中的隐式转换：
  prvalue → lvalue: 不可能！prvalue 没有地址
  lvalue → prvalue: 通过拷贝初始化（需要拷贝/移动构造函数）
  xvalue → lvalue: 不可能！xvalue 即将销毁
```

**lvalue 到 prvalue 的转换——值的"退化"**：

```cpp
int x = 42;
int y = x;        // x（lvalue）→ prvalue：拷贝初始化
                   // 实际发生：读取 x 的值，用它初始化 y

std::string s1 = "hello";
std::string s2 = s1;  // s1（lvalue）→ 需要调用拷贝构造函数
// 这不是简单的值退化，而是调用了一个可能非常昂贵的函数

// 这就是为什么移动语义如此重要——它提供了 lvalue → rvalue 的"显式授权"路径：
std::string s3 = std::move(s1);  // s1 → xvalue → 调用移动构造函数
```

**数组和函数名的特殊退化规则**：

```cpp
// 数组到指针退化（array-to-pointer decay）
int arr[3] = {1, 2, 3};
int* p = arr;     // arr（lvalue，类型 int[3]）退化为 int*

// 但以下情况不退化：
decltype(arr) ref = arr;   // int(&)[3]——decltype 不退化
sizeof(arr);               // 12——sizeof 不退化
auto& r = arr;             // int(&)[3]——绑定引用不退化

// 函数到指针退化
void foo(int);
auto fp = foo;     // void(*)(int)——函数名退化为函数指针
auto& fr = foo;    // void(&)(int)——绑定引用不退化
```

## 函数返回值的值类别

函数的返回类型决定了其调用表达式的值类别，这对性能和正确性都有直接影响。

```cpp
// 1. 返回非引用类型 → prvalue
std::string make_string() {
    return "hello";   // prvalue——C++17 允许直接在调用处构造
}

// 2. 返回左值引用 → lvalue
std::string& get_ref(std::string& s) {
    return s;   // lvalue——可以取地址、赋值
}
get_ref(s) = "modified";  // ✅ 可以出现在赋值左侧

// 3. 返回右值引用 → xvalue
std::string&& get_rref(std::string& s) {
    return std::move(s);   // xvalue——可以触发移动
}
// ⚠️ 返回局部变量的右值引用是 UB：
std::string&& bad() {
    std::string s = "local";
    return std::move(s);  // ⚠️ UB：s 在函数返回后销毁
}

// 4. 返回 const 引用 → lvalue（但不可赋值）
const std::string& get_cref(const std::string& s) { return s; }

// 5. 返回 auto 时的值类别推导
auto f1() { return 42; }           // 返回 int，prvalue
auto& f2() { static int x = 42; return x; }  // 返回 int&，lvalue
auto&& f3() { return 42; }         // 返回 int&&，xvalue
decltype(auto) f4() { return (42); } // ⚠️ 编译失败：(42) 是 prvalue，无法绑定到 int&&
                                     // 但编译器可能接受：decltype(auto) 推导为 int
```

**返回值的常见模式**：

```cpp
// 模式 1：返回 by value——优先用这个
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return v;   // NRVO 或移动——零拷贝
}

// ⚠️ 模式 2：不要对返回值用 std::move
std::string bad_return() {
    std::string s = "hello";
    return std::move(s);   // ❌ 阻碍 NRVO！比 return s 更差
}
// 原因：return std::move(s) 返回 xvalue，编译器必须用移动构造函数
//        而 return s 返回 prvalue，编译器可以直接在目标位置构造（guaranteed elision）

// 但对成员变量用 std::move 是正确的：
struct Holder {
    std::string data;
    std::string release() {
        return std::move(data);   // ✅ data 是成员，不会触发 NRVO
    }
};

// 模式 3：条件返回时 NRVO 可能被禁用
std::string conditional(bool flag) {
    std::string a = "hello";
    std::string b = "world";
    if (flag) return a;   // 两个不同的变量——NRVO 不适用
    return b;              // 编译器将使用移动（C++11+ 要求或隐式移动）
}
```

## 运算符重载与值类别

运算符的返回类型设计直接影响链式调用的能力和语义正确性。

```cpp
// 赋值运算符返回 *this 的引用——支持链式赋值
class MyString {
    std::string data_;
public:
    // 赋值运算符：返回左值引用
    MyString& operator=(const MyString& other) {
        data_ = other.data_;
        return *this;   // lvalue——支持 a = b = c
    }
    MyString& operator=(MyString&& other) noexcept {
        data_ = std::move(other.data_);
        return *this;   // 仍然返回 lvalue 引用
    }

    // 下标运算符：通常返回引用
    char& operator[](std::size_t i) { return data_[i]; }       // 非 const 版——可写
    const char& operator[](std::size_t i) const { return data_[i]; } // const 版——只读

    // 流运算符：返回左值引用以支持链式输出
    friend std::ostream& operator<<(std::ostream& os, const MyString& s) {
        return os << s.data_;  // 返回 os 的引用，支持 cout << a << b
    }

    // 算术运算符：通常返回 prvalue（新值）
    MyString operator+(const MyString& rhs) const {
        MyString result = *this;
        result.data_ += rhs.data_;
        return result;   // prvalue——不应修改操作数
    }

    // 前置自增返回 lvalue，后置自增返回 prvalue
    MyString& operator++() {    // 前置 ++i
        // 修改自身
        return *this;           // lvalue
    }
    MyString operator++(int) {  // 后置 i++（int 是占位符，区分前置/后置）
        MyString old = *this;
        // 修改自身
        return old;             // prvalue（旧值的拷贝）
    }
};
```

**隐式 this 的值类别——引用限定符（C++11）**：

```cpp
class Data {
    std::vector<int> items_;
public:
    // C++11 引用限定符：控制 *this 的值类别
    std::vector<int>& get() & { return items_; }           // *this 是 lvalue 时调用
    std::vector<int>  get() && { return std::move(items_); } // *this 是 rvalue 时调用

    // 常见于 optional::value()：
    // T& value() &;
    // const T& value() const&;
    // T&& value() &&;
    // const T&& value() const&&;
};

Data d;
auto& v1 = d.get();               // 调用 & 版本——v1 是引用
auto v2 = Data{}.get();            // 调用 && 版本——移动所有权
```

## 引用绑定规则

引用绑定规则决定了什么样的表达式可以绑定到什么样的引用，这是值类别系统最直接的应用场景。

**绑定优先级（从最严格到最宽松）**：

```cpp
int x = 42;
const int cx = 42;

// 1. 非 const 左值引用 T& —— 只绑定到非 const lvalue
int& r1 = x;          // ✅
// int& r2 = cx;      // ❌ 不能丢掉 const
// int& r3 = 42;      // ❌ 不能绑定 prvalue
// int& r4 = std::move(x); // ❌ 不能绑定 xvalue

// 2. const 左值引用 const T& —— 绑定到任何东西
const int& r5 = x;              // ✅ lvalue
const int& r6 = cx;             // ✅ const lvalue
const int& r7 = 42;             // ✅ prvalue（实体化 + 生命周期延长）
const int& r8 = std::move(x);   // ✅ xvalue（生命周期延长）

// 3. 右值引用 T&& —— 绑定到 rvalue（prvalue 和 xvalue）
// int&& r9 = x;                 // ❌ 不能绑定 lvalue
int&& r10 = 42;                  // ✅ prvalue
int&& r11 = std::move(x);       // ✅ xvalue
// int&& r12 = cx;               // ❌ 不能绑定 const lvalue

// 4. const 右值引用 const T&& —— 绑定到 const rvalue
const int&& r13 = 42;           // ✅ 但极少使用
// 主要用途：防止意外移动的右值引用重载
```

**用户自定义类型的引用绑定**：

```cpp
struct Base {};
struct Derived : Base {};

Derived d;

// 派生类到基类的引用绑定
Base& rb = d;              // ✅ lvalue 绑定（隐式派生到基类转换）
const Base& crb = d;       // ✅
Base&& rrb = Derived{};    // ✅ xvalue 绑定

// ⚠️ 类型必须完全匹配（引用绑定不会创建新对象）
// int& ri = double_val;   // ❌ 需要创建临时 int——不能绑定到非 const 引用
const int& cri = 3.14;     // ✅ const 引用可以绑定到转换结果
// 编译器会创建一个临时 int(3.14)，然后绑定 cri 到该临时对象
// 生命周期延长规则适用

// ⚠️ 基类引用绑定到派生类 rvalue 时的生命周期延长陷阱
struct Base {
    virtual ~Base() = default;
};
struct Derived : Base {
    std::string payload;
};
const Base& ref = Derived{};  // 临时 Derived 对象的生命周期延长
// ✅ 安全——延长的是整个 Derived 对象
```

## 拷贝消除（Copy Elision）

拷贝消除是值类别系统对性能影响最直接的地方。C++17 将其分为保证消除和可选消除两类。

**C++17 保证的拷贝消除（guaranteed copy elision）**：

```cpp
struct Widget {
    Widget() { std::puts("default"); }
    Widget(const Widget&) { std::puts("copy"); }
    Widget(Widget&&) { std::puts("move"); }
};

// 场景 1：prvalue 直接初始化
Widget w1 = Widget();  // 输出 "default"——无拷贝、无移动
// C++17：Widget() 是 prvalue，直接在 w1 的位置构造

// 场景 2：return prvalue
Widget make() {
    return Widget();    // 输出 "default"——直接在调用处构造
}

// 场景 3：prvalue 嵌套初始化
Widget w2 = Widget(Widget(Widget()));
// 输出 "default"——仅一次，所有中间 prvalue 被消除

// 场景 4：函数参数也是 prvalue
void take(Widget w) {}
take(Widget());  // Widget 在 take 的参数位置直接构造——零拷贝
```

**NRVO（Named Return Value Optimization）—— 仍是可选的**：

```cpp
Widget make_named() {
    Widget w;
    return w;   // NRVO 可能消除拷贝/移动，但不保证
}

// C++17 标准行为：
// 1. 如果编译器执行 NRVO → 直接在返回位置构造 w（零拷贝）
// 2. 如果编译器不执行 NRVO → w 被隐式移动（C++11 起保证）
//    但 Widget 需要可移动（如果移动构造函数被删除则拷贝）

// NRVO 的限制——以下情况 NRVO 不适用：
Widget make_two(bool flag) {
    Widget a, b;
    if (flag) return a;  // 两个不同的命名变量——NRVO 可能不适用
    return b;
}

// 返回函数参数——NRVO 也不适用：
Widget pass_through(Widget w) {
    return w;  // 参数不是局部变量——不适用 NRVO，但会隐式移动
}
```

**C++17 前后的对比**：

```
C++17 之前（简化视图）：
  Widget w = Widget();
  1. Widget() 创建临时对象 T1
  2. T1 拷贝/移动构造到 w（编译器优化可能省略）
  3. T1 析构

C++17 之后：
  Widget w = Widget();
  1. Widget() 直接在 w 的位置构造
  2. 完成。无临时对象，无需优化。

影响：之前依赖拷贝构造函数有副作用的代码在 C++17 前后行为不同。
      如果你的拷贝构造函数做了可观测的操作（打印、计数等），
      C++17 的保证消除会改变行为。
```

## 常见陷阱与教训

**陷阱 1：对已移动对象继续使用**：

```cpp
std::string s = "hello";
std::string t = std::move(s);
// s 现在处于"合法但未指定"状态
std::cout << s.size();   // ⚠️ 结果不可预测（可能是 0，也可能是 5）
std::cout << s;           // ⚠️ 可能输出空串，也可能输出别的
s.clear();                // ✅ 安全操作
s = "new value";          // ✅ 赋值是安全的
```

**陷阱 2：悬空引用与返回值类别**：

```cpp
// 返回局部变量的引用——UB
std::string& bad() {
    std::string s = "hello";
    return s;   // ❌ 编译器通常会警告
}

// 返回局部变量的右值引用——同样是 UB
std::string&& also_bad() {
    std::string s = "hello";
    return std::move(s);   // ❌ 仍然是 UB
}

// 正确：返回 by value
std::string good() {
    std::string s = "hello";
    return s;   // ✅ 移动或 NRVO
}
```

**陷阱 3：`auto` 推导丢失值类别信息**：

```cpp
std::string get();

auto a = get();           // std::string——prvalue 被拷贝（C++17 前）/直接初始化
auto& b = get();          // ❌ 编译错误：不能将 prvalue 绑定到非 const 引用
const auto& c = get();    // ✅ const 引用延长临时对象寿命
auto&& d = get();         // ✅ 万能引用，绑定到实体化后的临时对象
decltype(auto) e = get(); // std::string——prvalue → 非引用类型
```

**陷阱 4：列表初始化与值类别**：

```cpp
std::vector<int> v1{1, 2, 3};  // 初始化列表构造
std::vector<int> v2 = {1, 2, 3};  // 同上（拷贝列表初始化）

// 但注意：
auto v3 = {1, 2, 3};  // std::initializer_list<int>，不是 vector！
auto v4{1, 2, 3};      // C++17：std::initializer_list<int>
auto v5 = 1;           // int

// initializer_list 的特殊性：
// initializer_list 的元素始终是 const——不能从中移动
std::vector<std::string> v6 = {"hello", "world"};
// 每个字符串字面量被拷贝构造到 vector 中——无法移动
// C++26 的 std::inplace_vector 和 P2243 可能改善此问题
```

**陷阱 5：完美转发的编译错误信息**：

```cpp
template <typename T>
void call_target(T&& arg) {
    target(std::forward<T>(arg));
}

// 当 target 没有匹配的重载时，错误信息会非常冗长
// 因为编译器会在模板实例化的上下文中报告错误
// C++20 Concepts 可以改善这一点：
template <typename T>
concept Callable = requires(T t) { target(std::forward<T>(t)); };

template <Callable T>
void call_target(T&& arg) {
    target(std::forward<T>(arg));
}
```

**陷阱 6：条件表达式的值类别**：

```cpp
int a = 1, b = 2;

// 三元运算符的值类别取决于两个分支
auto&& r1 = (true ? a : b);    // int&——两个都是 lvalue → lvalue
auto&& r2 = (true ? a : 42);   // int——一个是 prvalue → prvalue（执行类型转换）
auto&& r3 = (true ? a : std::move(b)); // int——lvalue + xvalue → prvalue

// ⚠️ 这意味着条件表达式可能意外产生拷贝：
std::string x = "hello", y = "world";
std::string z = true ? x : std::move(y);
// 结果：z 是 x 的拷贝（不是移动！因为条件表达式整体是 prvalue，但 x 需要拷贝到结果中）
```

**陷阱 7：引用成员与值类别的组合陷阱**：

```cpp
struct Trap {
    const std::string& ref;
};

Trap make_trap() {
    std::string s = "temp";
    return Trap{s};   // ⚠️ 拷贝 Trap 时，ref 可能指向已销毁的临时对象
}

Trap t = make_trap();
// make_trap() 返回的 Trap 包含悬空引用
// 即使编译器执行 NRVO，s 在 make_trap() 返回后也已销毁
```

## 延伸阅读

- [值类别术语解释](/topics/cpp-jargon/value-categories) — 五个值类别的简明定义与速查表
- [移动语义与性能优化](/topics/performance) — 移动语义、SSO、缓存友好的实践指南
- [RAII 与资源管理](/topics/raii) — Rule of 5、智能指针、scope guard
- [模板元编程](/topics/template-metaprogramming) — SFINAE、Concepts、完美转发的模板技术
- C++ 标准 [\[expr.type\]](https://eel.is/c++draft/expr.type) — 值类别的正式定义
- C++ 标准 [\[class.temporary\]](https://eel.is/c++draft/class.temporary) — 临时对象的创建与销毁规则
- cppreference [Value categories](https://en.cppreference.com/w/cpp/language/value_category) — 权威参考
- Nicolai Josuttis, *C++ Move Semantics* — 移动语义的全面解读
- Arthur O'Dwyer, ["A Brief Introduction to `std::move` and `std::forward`"](https://quuxplusone.github.io/blog/2022/01/23/move-forward/) — 精准剖析 move/forward 的常见误解
