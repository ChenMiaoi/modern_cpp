---
title: 重载决议
topic: topics
feature: overload-resolution
status_checked_at: 2026-06-02
standard: N/A
---

# 重载决议

## 概述

重载决议（overload resolution）是编译器在函数调用表达式中，从一组同名候选函数中选出**最佳匹配**的过程。这是 C++ 类型系统与接口设计的核心机制——它决定了 `std::cout << 42` 调用哪个 `operator<<`，`std::move(x)` 是否会意外劫持某个对象，以及为什么你的完美转发包装器在某些调用点爆炸。

标准将整个过程分为三步：

1. **建立候选函数集**（candidate functions）
2. **筛选可行函数**（viable functions）——参数数量匹配且存在隐式转换序列
3. **选取最佳可行函数**（best viable function）——逐参数比较转换质量

如果第三步无法决出唯一胜者，调用**歧义**（ill-formed）。

## 候选函数集的构建

候选函数来自以下来源，按作用域层级组织：

```cpp
namespace N {
    void f(int);           // ① 命名空间作用域
    struct X {
        void f(double);    // ② 成员函数
        friend void f(long); // ③ 友元函数（注入外围命名空间）
    };
}

void test(N::X x) {
    x.f(42);   // 候选：X::f(double) + 命名空间 N 中的 f + ADL 找到的 f
    f(42);     // 候选：普通查找 N::f(int) + ADL 找到的 f(long)
}
```

候选函数的来源：

- **普通查找**（unqualified lookup）：沿作用域链向上查找，找到即停止外层查找（name hiding）
- **实参依赖查找**（ADL）：根据实参类型的关联命名空间和类查找
- **运算符**：对 `a @ b`，候选包括成员 `a.operator@(b)` 和非成员 `operator@(a, b)`，通过普通查找和 ADL 合并

```cpp
namespace lib {
    struct Widget {};
    void serialize(Widget, int);  // ADL 找到
}

void test() {
    lib::Widget w;
    serialize(w, 10); // 普通查找找不到 → ADL 在 lib 中找到
}
```

## 可行函数筛选

从候选集中保留**可行函数**，条件：

1. **参数数量匹配**：调用有 N 个实参，函数形参数量 ≤ N，缺失的必须有默认值
2. **隐式转换序列存在**：每个实参到对应形参都能构造出隐式转换序列

```cpp
void f(int, int = 0);     // 可行：f(1) 和 f(1,2)
void f(int, int, int);    // 不可行：f(1) 实参不足
void f(const char*);      // 不可行：f(1) 不存在 int → const char* 隐式转换
```

## 隐式转换序列

每个实参到形参的转换由**隐式转换序列**（implicit conversion sequence, ICS）描述。一个 ICS 最多包含三段：

```
标准转换序列：  标准转换 → 标准转换 → 标准转换
用户定义转换序列：标准转换 → 用户定义转换（构造或转换运算符） → 标准转换
省略号转换序列：  （任何实参 → ...）
```

这三类之间**永远**存在全序：标准转换序列 > 用户定义转换序列 > 省略号转换序列。

## 标准转换序列

标准转换序列是最常见的情况，由零到一个标准转换的链组成。标准转换分为四大类：

### 左值变换

将左值转为右值、函数/数组到指针的退化：

```cpp
int x = 42;
int& ref = x;
int val = ref;   // lvalue-to-rvalue：读取 ref 的值

void g(int*);
int arr[3];
g(arr);          // array-to-pointer：arr 衰退为 int*

void h(int(&)(int));
int foo(int);
h(foo);          // function-to-pointer
```

### 数值提升与转换

- **提升**（promotion）：`char`/`short` → `int`，`float` → `double`——值不变但类型"更宽"
- **转换**（conversion）：`int` → `double`、`double` → `int`、指针间转换等——可能丢失信息

```cpp
void f(int);      // ①
void f(double);   // ②

f('a');            // ① 胜出：char → int 是提升，int → double 是转换
f(3.14);           // ② 胜出：精确匹配 double
```

提升**严格优于**转换——这是 `f('a')` 选 `f(int)` 而非 `f(double)` 的原因。

### 限定转换

在指针/引用上添加 `const`/`volatile` 限定符：

```cpp
int* p = nullptr;
const int* cp = p;          // int* → const int*：限定转换，合法
int* q = cp;                // const int* → int*：去掉限定，不合法

void foo(const int&);
foo(/* int& */);            // int& → const int&：限定转换，标准转换序列的一部分
```

限定转换有严格的层级规则：只能添加限定，不能去掉；多层指针的限定必须逐层一致（`const int**` 不能隐式转为 `int**`，但 `int**` 可以转为 `const int* const*`）。

### 完整的标准转换序列结构

```
[左值变换] → [提升 或 转换] → [限定转换]
```

每一段都可以为空。一个什么都不做的转换（同类型间）是**精确匹配**（exact match），排序最高。

## 用户定义转换序列

当标准转换不够用时，编译器尝试通过**转换运算符**或**转换构造函数**进行一步用户定义转换。整个序列为：

```
标准转换 → 用户定义转换 → 标准转换
```

```cpp
struct Meter {
    double value;
    Meter(double v) : value(v) {}  // 转换构造函数
};

struct Foot {
    double value;
    operator double() const { return value * 0.3048; }  // 转换运算符
};

void print(Meter m);

Foot f{10};
print(f);
// ICS: Foot → double（用户定义转换：Foot::operator double）
//      → Meter（用户定义转换：Meter::Meter(double)）
//
// 两个用户定义转换 → 不是单一隐式转换序列 → 不可行！
// 只允许一段用户定义转换
```

关键规则：**整个隐式转换序列中，用户定义转换最多只能有一段**。如果需要两段（类型 A → 类型 B → 类型 C，两步各需一次用户定义转换），编译器报错。

### 转换运算符的隐式调用

```cpp
struct StringWrapper {
    std::string s;
    operator const char*() const { return s.c_str(); }
};

void log(const char*);
void log(const std::string&);

StringWrapper w{"hello"};
log(w);  // 歧义？operator const char* 是用户定义转换
         // std::string 的转换构造函数也是用户定义转换
         // 两者都需要一步用户定义 → 歧义
```

C++11 的 `explicit` 转换运算符只在显式上下文中使用，消除了意外的隐式转换：

```cpp
struct SafeBool {
    explicit operator bool() const { return true; }
};

SafeBool b;
if (b) { }          // OK：bool 上下文是显式转换上下文
bool flag = b;       // OK：direct-initialization 也是显式上下文
bool flag2 = {b};    // 错误：copy-initialization 不是显式上下文
int x = b;           // 错误：bool → int 也需要用户定义转换，不能链式
```

## 省略号转换序列

省略号转换（`...`）是最后手段——任何实参都能匹配，但质量最低：

```cpp
void f(int);     // 标准转换序列
void f(...);     // 省略号转换序列

f(42);           // 选 f(int)：标准转换严格优于省略号
```

省略号转换不检查类型兼容性，可能导致运行时未定义行为——这正是 `printf` 系列函数脆弱性的根源。

## 转换序列的排序

最佳可行函数的选取是**逐参数比较**：对每个实参，比较对应 ICS 的质量。

### 排序规则

1. **不同类别**：标准转换序列 > 用户定义转换序列 > 省略号转换序列

2. **同为标准转换序列**：
   - 精确匹配（无需任何转换）最佳
   - 提升优于普通转换
   - 其余按标准转换的类型层级比较

3. **同为用户定义转换序列**：比较用户定义转换前后的标准转换部分——但通常无法区分两个不同的用户定义转换（只能区分环绕它们的标准转换）

4. **同为精确匹配时的微妙区别**：
   - 非限定转换优于限定转换（`int` → `int` 优于 `int` → `const int`）
   - `bool` 转换中，指针/整数到 `bool` 的转换劣于其他精确匹配

```cpp
void g(int*);            // ①
void g(const int*);      // ②

int x = 0;
g(&x);  // ① 胜出：①是精确匹配（int* → int*），②需要限定转换（int* → const int*）
```

### F1 在所有参数上都优于 F2 时

```cpp
void f(int, double);     // ①
void f(double, int);     // ②

f(1, 1);  // 歧义！
          // 第1个参数：①精确匹配(int→int) 优于 ②转换(int→double) → ①胜
          // 第2个参数：②精确匹配(int→int) 优于 ①转换(int→double) → ②胜
          // 没有一个函数在所有参数上都胜出 → 歧义
```

### 完美匹配的额外偏好

对于精确匹配（不涉及数值转换），编译器还会偏好：

- 绑定到引用时，直接绑定优于通过临时对象绑定
- 对于模板特化和非模板函数，非模板函数通常优先（见下文）

## 函数模板的偏序

当两个函数模板都可以匹配时，编译器通过**偏序**（partial ordering）决定哪个更特化。这是将两个模板互相用作对方形参推导的过程：

```cpp
template <typename T>
void f(T);              // ① 更通用

template <typename T>
void f(T*);             // ② 更特化

int* p;
f(p);  // ② 胜出：T* 比 T 更特化
```

偏序规则：
1. 用 `f1` 的形参生成唯一合成类型，代入 `f2` 做推导
2. 用 `f2` 的形参生成唯一合成类型，代入 `f1` 做推导
3. 若只有一方推导成功，成功的一方更特化
4. 若都成功或都失败，无法比较

```cpp
template <typename T>
void h(T, T);           // ①

template <typename T, typename U>
void h(T, U);           // ②

h(1, 2);    // ① 胜出：①比②更特化（两个参数类型必须相同）
h(1, 2.0);  // ② 胜出：①推导 T=int 和 T=double 矛盾，不可行
```

偏序在两个维度上排序：模板参数的**数量**（更少参数更特化）和**形式**（更具体的模式更特化）。

## 约束与概念（C++20）

C++20 引入概念（concepts）后，约束成为重载决议的**先决条件**——在比较 ICS 之前，先检查约束是否满足。约束检查位于候选筛选阶段之后、ICS 比较之前：

```
候选集 → 可行函数筛选 → 约束检查 → ICS 比较 → 最佳可行
```

```cpp
#include <concepts>

template <typename T>
void process(T x) requires std::integral<T> {
    // 处理整数
}

template <typename T>
void process(T x) requires std::floating_point<T> {
    // 处理浮点数
}

process(42);       // 只有第一个约束满足 → 选第一个
process(3.14);     // 只有第二个约束满足 → 选第二个
```

### 约束的偏序

当多个约束的函数签名相同时，编译器比较约束强度：

```cpp
template <typename T> requires std::integral<T>
void serialize(T);                          // 约束 A

template <typename T> requires (std::integral<T> && std::signed_integral<T>)
void serialize(T);                          // 约束 B：A ∧ 额外条件

serialize(42);       // 选第二个：B 蕴含 A（更特化），但 A 不蕴含 B
serialize(42u);      // 选第一个：unsigned 不满足 signed_integral，B 不满足
```

## 子包含规则

约束的比较基于**合取范式**（conjunctive normal form）的子包含关系（subsumption）：

- 约束 P **子包含**（subsumes）约束 Q，当且仅当 P 的合取子句是 Q 的合取子句的超集
- 子包含关系是偏序：P 子包含 Q 意味着 P 比 Q 更特化

```cpp
// 约束原子
template <typename T> concept C1 = std::integral<T>;
template <typename T> concept C2 = std::signed_integral<T>;
template <typename T> concept C3 = C1<T> && C2<T>;  // C3 = {integral ∧ signed_integral}

// C3 子包含 C1（因为 C1 ⊂ C3 的合取子句集）
// C3 子包含 C2
// C1 不子包含 C3
// C2 不子包含 C1（signed_integral 不蕴含 integral 的全集——实际上 integral 包含 signed 和 unsigned）
// 注意：子包含只看语法结构，不做语义蕴含分析
```

关键限制：子包含只对原子约束和合取（`&&`）进行结构比较。**析取**（`||`）不参与子包含判定：

```cpp
template <typename T> concept A = std::integral<T>;
template <typename T> concept B = std::floating_point<T>;
template <typename T> concept C = A<T> || B<T>;  // 析取

// C 不子包含 A（析取不出现在子包含分析中）
// A 也不子包含 C
// 两者无法区分 → 若签名相同则歧义
```

### 约束子包含的实际效果

```cpp
template <typename T> requires std::regular<T>
void sort(T* arr, std::size_t n);                     // ①

template <typename T> requires std::totally_ordered<T>
void sort(T* arr, std::size_t n);                     // ②

// totally_ordered 子包含 regular 的前提是 regular 定义中包含 totally_ordered
// 如果 regular = default_initializable + movable + copyable + equality_comparable + totally_ordered
// 则 regular 约束集包含 totally_ordered → regular 子包含 totally_ordered
// 因此 ① 更特化（regular 是更多约束的合取）
```

## SFINAE 与重载决议

SFINAE（Substitution Failure Is Not An Error）是 C++98 起就存在的核心机制：模板参数替换失败时，该重载被静默移除候选集，而非报错。

### SFINAE 的位置

SFINAE 发生在**候选函数构建**阶段——替换函数签名中的模板参数时，若产生无效类型或表达式，该候选被丢弃：

```cpp
template <typename T>
auto size(const T& c) -> decltype(c.size()) {
    return c.size();           // 有 size() 成员时才参与重载
}

template <typename T>
std::size_t size(const T* arr, std::size_t n) {
    return n;                  // 指针走这条路
}

int a[5];
size(a, 5);                    // 数组退化为指针，第二个候选匹配
std::vector<int> v;
size(v);                       // 第一个候选匹配
```

### enable_if 模式

```cpp
// C++11/14 风格
template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
std::string to_string(T val) {
    return std::to_string(val);  // 整数专用
}

// C++17 简化
template <typename T>
std::string to_string(T val) requires std::is_integral_v<T> {
    return std::to_string(val);  // C++20：等价效果，更清晰
}
```

### SFINAE 的适用范围

SFINAE 只对函数签名直接涉及的替换失败有效。**函数体**内的错误不是 SFINAE——它们是硬编译错误：

```cpp
template <typename T>
auto f(T x) -> decltype(x + x) {
    T::nonexistent_member();   // 函数体错误，不是 SFINAE → 硬错误
}
```

## 默认参数与重载决议

默认参数的交互非常微妙——它们影响**可行性**筛选，但不影响转换序列的**质量**比较。

```cpp
void f(int);              // ①
void f(int, int = 0);     // ②

f(42);  // 歧义！①和②都可行，①对唯一的实参做精确匹配
        // ②对第一个实参也做精确匹配
        // 两者 ICS 质量完全相同 → 无法区分 → 歧义
```

这个例子表明：拥有默认参数并不意味着"更差"的匹配。默认参数只帮助候选成为可行函数，不参与质量排序。

```cpp
void g(long);             // ①
void g(int, int = 0);     // ②

g(42);  // ② 胜出：②精确匹配(int→int)，①需要转换(int→long)
```

## 删除函数与重载决议

删除函数（`= delete`）参与重载决议的全流程——它在候选集中、可以被选为最佳匹配，但使用时报错。这与"函数不存在"有本质区别：

```cpp
void process(int);                        // ①
void process(double) = delete;            // ② 删除但存在于候选集

process(42);     // OK：选 ①（精确匹配 int）
process(3.14);   // 错误：选 ②（精确匹配 double）→ 使用了删除函数
process('c');    // OK：char → int 是提升，char → double 是转换，选 ①
```

如果 ② 完全不存在，`process(3.14)` 会隐式转换为 `int` 并调用 ①。删除函数精确地阻止了这种意外转换——这是 API 设计中非常有力的工具。

```cpp
// 经典用法：阻止意外的窄化转换
class SafeInt {
    int val;
public:
    SafeInt(int v) : val(v) {}
    SafeInt(long long) = delete;  // 阻止 long long 隐式切窄
};
```

## 运算符重载决议

运算符表达式 `a @ b` 的候选函数来源：

1. **成员候选**：`a.operator@(b)`（若 `a` 是类类型）
2. **非成员候选**：普通查找 + ADL 找到的 `operator@(a, b)`
3. **内置候选**：编译器对内置类型的内置运算符

```cpp
struct Vec {
    double x, y;
    Vec operator+(const Vec& o) const { return {x + o.x, y + o.y}; }
};

Vec a{1,2}, b{3,4};
a + b;  // 候选：Vec::operator+(const Vec&) — 成员版本
        //       operator+(Vec, Vec) — 未找到非成员版本
        //       内置 + — Vec 不是算术类型，不可行
        // → 选成员版本
```

### 隐式转换与运算符

```cpp
struct Meter {
    double v;
    Meter(double d) : v(d) {}       // 转换构造函数
    Meter operator+(Meter o) const { return {v + o.v}; }
};

Meter m{1.0};
m + 2.0;  // 2.0 隐式转换为 Meter → 调用 Meter::operator+(Meter)
2.0 + m;  // 错误！double 不是类类型，没有成员 operator+
           // 非成员 operator+(double, Meter) 也不存在
```

修复：将运算符定义为友元非成员函数：

```cpp
struct Meter {
    double v;
    Meter(double d) : v(d) {}
    friend Meter operator+(Meter a, Meter b) { return {a.v + b.v}; }
};

Meter m{1.0};
m + 2.0;   // OK：2.0 → Meter，调用 operator+(Meter, Meter)
2.0 + m;   // OK：同上
```

### 运算符候选的排序

当成员和非成员运算符同时存在时，标准转换序列质量决定胜负。但**额外的隐式转换**代价高昂——如果成员版本需要对左操作数做一步隐式转换，而非成员版本不需要，则非成员版本胜出（反之亦然）。

```cpp
struct A {
    operator int() const { return 0; }
    bool operator==(int) const { return true; }      // 成员
};
bool operator==(int, const A&) { return true; }      // 非成员

A a;
a == 0;   // 候选1：成员 operator==(int) — a 精确匹配 this，0 精确匹配 int
          // 候选2：非成员 operator==(int, const A&) — int(0) 精确匹配 int，a 隐式转为 const A&
          // 候选1 对左操作数零转换，候选2 对右操作数零转换
          // 候选1 更好：a 到 this 的引用绑定是精确匹配，优于 a 到 int 的用户定义转换
```

## 构造函数重载决议

构造函数的选择遵循标准的重载决议规则，但有一些特殊考量：

```cpp
struct Config {
    Config();                               // ① 默认构造
    Config(int port);                       // ② int 构造
    Config(std::string host, int port);     // ③ 双参数构造
    explicit Config(const char* host);      // ④ explicit，阻止隐式转换
};

Config a;                    // ① 默认构造
Config b(8080);              // ② 直接初始化
Config c("localhost", 8080); // ③
Config d = "localhost";      // 错误：④ 是 explicit，不能用于拷贝初始化
Config e{"localhost"};       // OK：直接列表初始化，explicit 允许
```

### explicit 构造函数的影响

`explicit` 构造函数在**拷贝初始化**上下文中不可用（因为拷贝初始化需要隐式转换），但在**直接初始化**和**列表初始化**中可用：

```cpp
struct Foo {
    explicit Foo(int) {}
};

Foo a = 42;    // 错误：拷贝初始化，explicit 构造函数不可用
Foo b(42);     // OK：直接初始化
Foo c{42};     // OK：直接列表初始化

void bar(Foo);
bar(42);       // 错误：需要隐式转换，explicit 阻止
bar(Foo{42});  // OK：显式构造
```

## 拷贝与移动构造函数的选择

当拷贝和移动构造函数都可行时，**右值优先绑定到右值引用**：

```cpp
struct Buffer {
    int* data;
    std::size_t size;

    Buffer(std::size_t n) : data(new int[n]), size(n) {}
    ~Buffer() { delete[] data; }

    // 拷贝构造
    Buffer(const Buffer& o) : data(new int[o.size]), size(o.size) {
        std::copy(o.data, o.data + o.size, data);
    }

    // 移动构造
    Buffer(Buffer&& o) noexcept : data(o.data), size(o.size) {
        o.data = nullptr;
        o.size = 0;
    }
};

Buffer a(100);
Buffer b(a);              // 拷贝：a 是左值 → const Buffer& 匹配
Buffer c(std::move(a));   // 移动：std::move(a) 是右值 → Buffer&& 优先

// 函数返回值：NRVO 优先于移动，移动优先于拷贝
Buffer make() {
    Buffer tmp(50);
    return tmp;  // 若 NRVO 适用，直接构造在目标位置
                 // 否则：tmp 作为返回值是右值 → 移动构造
}
```

### 引用限定符的交互

C++11 允许对成员函数添加引用限定符，影响重载决议：

```cpp
struct Data {
    std::string value;

    std::string& get() & { return value; }             // 左值对象
    std::string&& get() && { return std::move(value); } // 右值对象
};

Data d;
auto& s = d.get();          // 左值版本：返回左值引用
auto s2 = Data{"x"}.get();  // 右值版本：返回右值引用，可被移动构造
```

## 完美转发与重载集

完美转发（`std::forward`）保持实参的值类别，但它对重载决议有深刻影响——尤其是在构造函数与转发构造函数之间：

```cpp
template <typename T, typename... Args>
T create(Args&&... args) {
    return T(std::forward<Args>(args)...);
}

struct Widget {
    Widget(int) {}            // ①
    Widget(const Widget&) {}  // ② 拷贝构造
};

Widget w1(42);
Widget w2 = create<Widget>(w1);  // Args = Widget& → forward 返回 Widget&
                                   // 匹配 ② 拷贝构造，正确
Widget w3 = create<Widget>(std::move(w1));  // Args = Widget → forward 返回 Widget&&
                                              // 匹配移动构造
```

### 转发构造函数（C++11 惯用法）

```cpp
struct Wrapper {
    std::string name;
    int value;

    // 转发构造函数：接受任意参数，完美转发给成员
    template <typename Name, typename... Rest,
              typename = std::enable_if_t<
                  !std::is_same_v<std::decay_t<Name>, Wrapper> &&
                  std::is_constructible_v<std::string, Name>>>
    explicit Wrapper(Name&& name, Rest&&... rest)
        : name(std::forward<Name>(name)), value(std::forward<Rest>(rest)...) {}

    // 需要显式定义拷贝/移动构造，否则模板会"截获"
    Wrapper(const Wrapper&) = default;
    Wrapper(Wrapper&&) = default;
};
```

没有 `enable_if` 守卫时，`Wrapper` 的拷贝/移动操作可能被转发构造函数模板"偷走"——模板通常比非模板更优先是错误的直觉：实际上**非模板函数优先于模板**，但当模板提供了更精确的匹配时（如精确匹配左值引用），模板会胜出。

## 常见重载决议陷阱

### 陷阱一：隐式转换导致意外选择

```cpp
void process(std::string_view sv);  // ①
void process(const char* s);         // ②

process("hello");  // 选 ②：const char* 是精确匹配，std::string_view 需要用户定义转换
                   // 如果你期望走 string_view 路径，这里会令人困惑
```

### 陷阱二：按值捕获 vs 按引用捕获

```cpp
void f(int);        // ①
void f(int&);       // ②

int x = 42;
f(x);   // 歧义？实际上 ② 更精确：int& 直接绑定到 x
        // ① 需要 lvalue-to-rvalue 转换
        // → ② 胜出

f(42);  // ① 胜出：42 是右值，不能绑定到 int&
```

### 陷阱三：initializer_list 的优先级

```cpp
void f(std::initializer_list<int>);  // ①
void f(int);                          // ②

f({1});    // 选 ①：花括号列表优先匹配 initializer_list
f(1);      // 选 ②：没有花括号，不考虑 initializer_list
```

列表初始化有一个**特殊偏好**：当花括号列表可以直接匹配 `initializer_list` 参数时，它优先于其他接受相同元素类型的重载。这导致了很多令人困惑的行为：

```cpp
std::vector<int> v1(10, 1);   // 10 个元素，每个值为 1
std::vector<int> v2{10, 1};   // 2 个元素：10 和 1
// 原因：{10, 1} 匹配 initializer_list<int> 构造函数，而非 (size, value) 构造函数
```

### 陷阱四：模板与非模板的交互

```cpp
void f(int) {}                  // 非模板

template <typename T>
void f(T) {}                    // 模板

f(42);   // 选非模板：精确匹配时，非模板函数优先于模板实例化
f<int>(42);  // 显式指定模板参数 → 强制选择模板版本
```

### 陷阱五：基类成员隐藏

```cpp
struct Base {
    void f(int);
};

struct Derived : Base {
    void f(double);  // 隐藏 Base::f(int)
};

Derived d;
d.f(42);     // 调用 Derived::f(double)，不是 Base::f(int)
             // 名称查找在 Derived 中找到 f → Base 中的 f 不再是候选
             // 即使 Base::f(int) 是更精确的匹配

using Base::f;  // 解决方案：using 声明将 Base::f 引入作用域
d.f(42);        // 现在两个候选都在 → Base::f(int) 胜出
```

### 陷阱六：右值引用绑定意外

```cpp
void process(std::string&&);   // 只接受右值

std::string s = "hello";
process(s);                    // 错误：s 是左值，不能绑定到 string&&
process(std::move(s));         // OK：显式转为右值
// 警告：此时 s 已被移动，处于合法但未指定状态
```

### 陷阱七：auto 与重载的意外交互

```cpp
void log(const std::string&);  // ①
void log(int);                  // ②

auto x = 42;
log(x);        // 选 ②：x 推导为 int

auto y = "hello";
log(y);        // 选 ①：y 推导为 const char[6]，可以转为 string_view 但不能隐式转为 string
               // → 实际上是 const char* → std::string 的转换构造函数 → 用户定义转换
               // → 与 ② (const char* → int 标准转换) 比较？
               // 错误：const char* 不能标准转换为 int
               // → 只有 ① 可行
```

理解重载决议的关键在于始终问三个问题：候选有哪些？哪些可行？可行集中哪个在**每个参数**上都更好或至少不差？当没有函数满足"至少不差"的条件时，编译器拒绝调用——宁可报错，也不猜。
