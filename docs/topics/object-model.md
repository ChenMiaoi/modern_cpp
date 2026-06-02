---
title: C++ 对象模型
topic: topics
feature: object-model
standard: C++
status_checked_at: 2026-06-02
exercises: []
solutions: []
---

# C++ 对象模型

> C++ 对象模型是语言最底层的抽象：一块内存如何成为对象，对象如何布局，指针如何追踪对象身份，以及生命周期何时开始和结束。理解对象模型是理解 RAII、模板、多态和并发的前提。

---

## 对象创建与存储期

C++ 中每个对象都有**存储期**（storage duration），决定了其内存的分配和释放时机。

### 四种存储期

```cpp
// 1. 自动存储期（automatic）— 函数局部变量
void f() {
    int x = 42;          // x 在栈帧上，离开作用域即销毁
    std::string s = "hi"; // s 及其内部缓冲区——析构函数在 } 处调用
}

// 2. 静态存储期（static）— 全局变量、namespace 作用域、static 局部
int global = 100;              // 程序启动时初始化，程序结束时销毁
void g() {
    static int counter = 0;   // 首次调用时初始化，程序结束时销毁
    static std::mutex mtx;    // 同上——构造/析构顺序跨翻译单元有严格规则
}

// 3. 线程存储期（thread_local）— C++11
thread_local int tls_var = 0;  // 每个线程独立副本，线程退出时销毁

// 4. 动态存储期（dynamic）— new / delete
void h() {
    auto* p = new Widget{};    // 堆上分配，必须显式 delete
    delete p;
}
```

### 静态存储期的初始化顺序

```
┌──────────────────────────────────────────────────────────┐
│  静态存储期初始化——两阶段模型                              │
│                                                          │
│  阶段 1：零初始化（zero-initialization）                   │
│    所有具有静态存储期的变量在任何其他初始化之前先归零        │
│                                                          │
│  阶段 2：常量初始化 或 动态初始化                           │
│    · 常量初始化：constexpr 可求值 → 编译期完成              │
│    · 动态初始化：运行时执行构造函数/初始化表达式             │
│                                                          │
│  同一翻译单元内：按定义顺序初始化                           │
│  跨翻译单元：顺序未定义 → "静态初始化顺序惨案"              │
└──────────────────────────────────────────────────────────┘
```

---

## 对象表示与值表示

标准区分两个概念：

- **对象表示**（object representation）：对象占用的连续内存字节序列
- **值表示**（value representation）：决定对象值的那部分比特位

```cpp
// 大多数情况下二者相同，但位域打破了这种对齐
struct Flags {
    unsigned int active  : 1;  // 1 bit
    unsigned int visible : 1;  // 1 bit
    unsigned int padding : 30; // 30 bits — 填充到 32 bits
};

// sizeof(Flags) == 4（对象表示 = 4 字节）
// 值表示 = 32 bits 中的 32 bits（全部参与值决定）

// 对于 bool：
// sizeof(bool) == 1（对象表示 = 1 字节 = 8 bits）
// 值表示 = 1 bit（只使用 0 和 1，其余 7 bits 是填充位）
// 读取填充位 → 未定义行为
```

---

## 对齐与填充

每个类型都有**对齐要求**（alignment requirement），以字节为单位。

```
struct Example {
    char  a;    // offset 0, size 1
    // ---- 3 bytes padding（使 b 对齐到 4 的倍数）----
    int   b;    // offset 4, size 4
    char  c;    // offset 8, size 1
    // ---- 3 bytes padding（使整体大小为对齐的倍数）----
};
// sizeof(Example) == 12，alignof(Example) == 4

内存布局：
offset:  0    1    2    3    4    5    6    7    8    9   10   11
       ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
       │ a  │pad │pad │pad │ b  . b  . b  . b  │ c  │pad │pad │pad │
       └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
       ├─char─┤├──── 3B pad ───┤├──── int ─────┤├char┤├── 3B pad ──┤
```

```cpp
// C++11: alignas 控制对齐
struct alignas(64) CacheLine {
    char data[64];
};
// alignof(CacheLine) == 64 — 适合对齐到缓存行边界

// alignof 查询
static_assert(alignof(double) == 8);
static_assert(alignof(int)    == 4);
```

### 调整成员顺序减少填充

```cpp
// 浪费空间
struct Wasteful {
    char  a;   // +1, pad 3
    int   b;   // +4
    char  c;   // +1, pad 7
    double d;  // +8
};  // sizeof == 24

// 紧凑排列（按对齐需求降序）
struct Compact {
    double d;  // +8
    int    b;  // +4
    char   a;  // +1
    char   c;  // +1, pad 2
};  // sizeof == 16 — 节省 33%
```

---

## POD / Trivially Copyable / Standard Layout

标准将类型分为若干"平凡性"和"布局性"层次：

```
┌──────────────────────────────────────────────────────────────────┐
│  类型特征层次（C++17/20 简化视图）                                │
│                                                                  │
│  is_trivial = is_trivially_copyable + is_trivially_default_constructible
│                                                                  │
│  is_standard_layout:                                             │
│    · 所有非静态成员具有相同访问控制                                 │
│    · 没有虚函数/虚基类                                            │
│    · 所有非静态成员也是 standard_layout                           │
│    · 基类中最多一个有非静态数据成员                                │
│    · 首个成员类型与基类不同（C++17 放宽）                          │
│                                                                  │
│  is_trivially_copyable:                                          │
│    · 没有非平凡的拷贝/移动构造函数或赋值运算符                     │
│    · 没有非平凡的析构函数                                         │
│    · 可以 memcpy 安全复制                                         │
│                                                                  │
│  C++20: POD = is_trivial + is_standard_layout（概念化替代）       │
└──────────────────────────────────────────────────────────────────┘
```

```cpp
struct Pod { int x; float y; };             // trivial + standard layout
struct StdLayout { int x; virtual ~StdLayout(){} }; // standard layout? No — 有虚函数
struct Trivial { std::string s; };           // trivially copyable? No — string 非平凡

// 为什么关心这些？
// 1. trivially_copyable → 可安全 memcpy/memmove
// 2. standard_layout → 可与 C 互操作，指针可 reinterpret_cast
// 3. trivial → 未初始化时读取有定义行为（C++20 implicit object creation）
static_assert(std::is_trivially_copyable_v<Pod>);
static_assert(std::is_standard_layout_v<Pod>);
```

---

## 严格别名规则与 Type Punning

**严格别名规则**（strict aliasing rule）：程序不得通过不兼容类型的左值访问对象（`char*`/`unsigned char*`/`std::byte*` 除外，允许以字节粒度检查任何对象）。

```cpp
// ❌ 违反严格别名 — 未定义行为
float f = 3.14f;
int* ip = reinterpret_cast<int*>(&f);
int bits = *ip;  // UB: 通过 int* 访问 float 对象

// ❌ 同样是 UB — union type punning（C 允许，C++ 不保证）
union { float f; int i; } u;
u.f = 3.14f;
int bits = u.i;  // C++ 中是 UB（GCC/Clang 在严格别名优化下会出错）

// ✅ 正确做法 — 使用 memcpy
float f = 3.14f;
int bits;
std::memcpy(&bits, &f, sizeof(bits));  // 定义行为，编译器会优化掉拷贝

// ✅ C++20: std::bit_cast
int bits = std::bit_cast<int>(3.14f);  // 编译期友好，无 UB
```

---

## 对象身份与值身份

```cpp
// 对象身份（object identity）：对象在内存中的地址
int a = 1, b = 1;
&a != &b;          // 不同对象，即使值相同

// 值身份（value identity）：对象当前表示的值
a == b;             // 值相同

// 关键场景：引用绑定的是对象身份，不是值
struct Animal { virtual ~Animal() = default; };
struct Dog : Animal { void bark() {} };
Dog d;
Animal& ref = d;   // ref 绑定到 d 这个对象（身份）
// dynamic_cast<Dog&>(ref) 安全——因为 ref 的动态类型确实是 Dog

// std::string 的 SSO（Small String Optimization）使值身份问题复杂化：
// 短字符串的字符存储在对象自身的栈空间中，长字符串在堆上
// 但 s.data() 的指针在赋值后可能失效——它指向的是对象内部的存储
```

---

## 空基类优化（EBO）

C++ 保证不同对象的地址不同，但**基类子对象**可以与其他子对象共享地址。

```cpp
struct Empty {};
struct NonEmpty { int x; };

// 常规情况：sizeof(Empty) == 1（保证不同实例地址不同）
static_assert(sizeof(Empty) == 1);

// 空基类优化：基类子对象可以不占空间
struct Derived : Empty {
    int value;
};
// sizeof(Derived) == 4（不是 8），Empty 子对象的地址 == Derived 的地址

// 多个空基类
struct A {};
struct B {};
struct C : A, B {
    int value;
};
// sizeof(C) == 4 — 两个空基类都不占额外空间
// 但 &static_cast<A&>(c) != &static_cast<B&>(c) — 两个基类子对象地址不同
// 编译器给不同空基类分配不同偏移（各偏移 0 和 1 字节等）

// C++20: [[no_unique_address]] — 成员也可以享受 EBO
struct CompressedPair {
    [[no_unique_address]] Empty e;
    int value;
};
// sizeof(CompressedPair) == 4 — e 不占空间

// 实际应用：allocator 通常为空类
template <typename T, typename Alloc = std::allocator<T>>
class Vector {
    T* data_;
    std::size_t size_, capacity_;
    [[no_unique_address]] Alloc alloc_;  // 默认 allocator 不占空间
};
// sizeof(Vector<int>) == 24（三个指针），不是 32
```

```
EBO 内存布局：

普通继承：                        空基类优化：
┌───────────────────┐            ┌───────────────────┐
│ Empty 子对象 (1B) │            │ Empty+int (共享地址)│
│ padding (3B)      │            │ value (4B)         │
│ value (4B)        │            └───────────────────┘
└───────────────────┘             sizeof == 4
 sizeof == 8

[[no_unique_address]] 成员：
┌────────────────────────────┐
│ e (与 value 共享地址, 0B)  │
│ value (4B)                 │
└────────────────────────────┘
 sizeof == 4
```

---

## 虚函数与 vtable 布局

虚函数调用通过**虚表**（vtable）实现间接分派。每个含虚函数的类有一个静态 vtable，每个含虚函数的对象持有一个指向 vtable 的指针（vptr）。

```cpp
class Base {
public:
    virtual void f() { /* ... */ }
    virtual void g() { /* ... */ }
    virtual ~Base() = default;
    int x = 0;
};

class Derived : public Base {
public:
    void f() override { /* ... */ }
    virtual void h() { /* ... */ }
    int y = 0;
};
```

```
Base 对象布局：                 Derived 对象布局：

┌──────────────┐              ┌──────────────┐
│ vptr ──────────┐            │ vptr ──────────┐
├──────────────┤  │            ├──────────────┤  │
│ x (4B)       │  │            │ x (4B)       │  │
├──────────────┤  │            ├──────────────┤  │
│ padding (4B) │  │            │ y (4B)       │  │
└──────────────┘  │            └──────────────┘  │
sizeof(Base)==16  │            sizeof(Derived)==16│
                  ▼                               ▼
Base::vtable:     Derived::vtable:
┌──────────────────┐  ┌──────────────────┐
│ &Base::f()       │  │ &Derived::f()    │ ← 覆写
├──────────────────┤  ├──────────────────┤
│ &Base::g()       │  │ &Base::g()       │ ← 继承
├──────────────────┤  ├──────────────────┤
│ &Base::~Base()   │  │ &Derived::~Derived() │
├──────────────────┤  ├──────────────────┤
│ typeinfo (RTTI)  │  │ &Derived::h()    │ ← 新增
│                  │  ├──────────────────┤
└──────────────────┘  │ typeinfo (RTTI)  │
                      └──────────────────┘
```

```cpp
// 虚调用的开销：
// 1. 一次指针解引用（读 vptr → 读 vtable 条目）
// 2. 间接跳转（阻止内联——编译器通常不能在编译期确定目标）
// 3. 每个对象多一个指针的存储（vptr）

// 去虚拟化（devirtualization）：当编译器能在编译期确定动态类型时
Derived d;
Base& b = d;
b.f();  // 编译器可能直接调用 Derived::f()，跳过 vtable 查找

// C++17: final 类阻止进一步继承 → 有利于去虚拟化
class FinalDerived final : public Base {
    void f() override final { /* ... */ }
};
```

---

## 动态对象创建：new/delete 内部机制

```cpp
// operator new 的调用链：
Widget* p = new Widget(args);
// 等价于：
void* raw = ::operator new(sizeof(Widget));  // 1. 分配原始内存
Widget* p = new(raw) Widget(args);            // 2. placement new — 构造

// operator delete：
delete p;
// 等价于：
p->~Widget();           // 1. 析构
::operator delete(p);   // 2. 释放内存

// 自定义全局 operator new/delete（通常只在特殊场景使用）
void* operator new(std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc{};
    return ptr;
}
void operator delete(void* ptr) noexcept {
    std::free(ptr);
}
```

调用 `operator new` 分配原始内存 → 在该内存上 placement new 构造对象 → 成功返回指针；构造抛异常时自动调用 `operator delete` 释放内存（不泄漏）。`operator new` 失败时调用 `new_handler`；若为 `nullptr` 则抛 `std::bad_alloc`。

---

## Placement new

```cpp
#include <new>  // placement new 所需头文件

// 1. 在预分配的缓冲区中构造对象
alignas(Widget) char buf[sizeof(Widget)];
Widget* w = new(buf) Widget(args);
// w 指向 buf，对象在 buf 中就地构造
w->~Widget();  // 必须手动析构——不能 delete（buf 不是 new 分配的）

// 2. 容器的典型用法：分配与构造分离
//  先用 ::operator new 分配原始内存，再用 placement new 逐个构造
//  C++20: std::construct_at / std::destroy_at（constexpr 友好）
auto* p = std::construct_at(ptr, args...);
std::destroy_at(p);  // C++17
```

---

## std::launder 与指针溯源

`std::launder`（C++17）解决的问题：当对象被原地替换（如 placement new 覆盖）后，指向旧对象的指针可能被编译器优化为仍指向旧值。

```cpp
struct X { const int n; };

X x{42};
X* p = &x;

// 在 x 所在内存重新构造
new (&x) X{100};

// ❌ 未定义行为：p 可能被编译器优化为仍读取 42
int val = p->n;

// ✅ 正确：通过 launder 获取新对象的指针
int val = std::launder(p)->n;  // 保证读取 100

// 何时需要 launder：
// 1. 对象生命周期结束后，在同一地址创建新对象
// 2. 新旧对象类型相同但有 const 成员或引用成员
// 3. 指针值不变但指向的对象已"更换"
//
// 何时不需要：
// 1. 对象生命周期未结束（只是修改了值）
// 2. 通过 placement new 返回的指针（已经是新对象的指针）
// 3. 通过 std::start_lifetime_as（C++23）获取的指针
```

---

## 对象生命周期规则

### 生命周期的开始与结束

```
对象生命周期开始：
  · 类型为 trivially copyable + implicit lifetime → 隐式创建（见下文）
  · 否则 → 构造函数完成时

对象生命周期结束：
  · 析构函数调用开始时
  · 或存储被释放/重用时（如 placement new 覆盖）

已结束生命周期的对象：
  · 名称仍然在作用域中，但指向该对象的引用/指针变为无效
  · 通过无效指针读取 → UB（除非 trivially copyable + implicit lifetime）
```

### C++20 生命周期规则变更

```cpp
// C++20 之前：析构后访问 const 成员是 UB
// C++20 放宽了 trivially destructible 对象的规则

struct Trivial { int x; };  // trivially destructible
Trivial t{42};
t.~Trivial();  // 生命周期结束

// C++20 之前：UB
// C++20：如果满足 implicit object creation 的条件，行为定义
//        但仍不推荐——使用 std::launder 或 construct_at 更安全
```

---

## 隐式对象创建（C++20）

C++20 引入**隐式对象创建**（implicit object creation）：某些操作会自动在原始存储中创建对象，使指针合法化。

```cpp
// 核心思想：以下操作如果需要，会隐式创建对象
// · malloc / calloc / realloc / aligned_alloc
// · operator new / operator new[]
// · std::allocator::allocate
// · std::start_lifetime_as（C++23）
// · memcpy / memmove / memset 等（C++20 起）

// 示例：malloc 后直接使用
struct S { int x; };
void* raw = std::malloc(sizeof(S));
S* p = static_cast<S*>(raw);    // C++20：隐式创建了一个 S 对象
p->x = 42;                       // 定义行为！（C++20 之前是 UB）

// 但有限制：隐式创建只发生在满足对齐要求的存储中
// 且只能创建 trivially copyable 类型的对象

// 多对象的隐式创建
struct Header { int size; };
struct Payload { char data[64]; };
struct Packet { Header h; Payload p; };

void* buf = std::malloc(sizeof(Packet));
Packet* pkt = static_cast<Packet*>(buf);
pkt->h.size = 42;  // C++20：隐式创建整个 Packet → Header 和 Payload 也被隐式创建
```

---

## 综合示例：手写 fixed_capacity_vector

```cpp
#include <new>
#include <memory>
#include <type_traits>

template <typename T, std::size_t N>
class fixed_capacity_vector {
    // 存储：足够大的对齐缓冲区
    alignas(T) unsigned char buf_[N * sizeof(T)];
    std::size_t size_ = 0;

public:
    static_assert(std::is_nothrow_destructible_v<T>,
        "T must be nothrow destructible for exception safety");

    fixed_capacity_vector() = default;

    ~fixed_capacity_vector() {
        clear();
    }

    void push_back(auto&&... args) {
        if (size_ >= N) throw std::length_error("full");
        std::construct_at(data() + size_++, std::forward<decltype(args)>(args)...);
    }

    void pop_back() {
        if (size_ == 0) return;
        std::destroy_at(data() + --size_);
    }

    void clear() {
        std::destroy_n(data(), size_);
        size_ = 0;
    }

    T& operator[](std::size_t i) { return data()[i]; }
    const T& operator[](std::size_t i) const { return data()[i]; }
    std::size_t size() const { return size_; }

private:
    T* data() { return std::launder(reinterpret_cast<T*>(buf_)); }
    const T* data() const { return std::launder(reinterpret_cast<const T*>(buf_)); }
};
```

---

## 常见误区

1. **`sizeof` 包含填充，`alignof` 不总是等于 `sizeof`**。`sizeof(long double)` 可能是 8、12 或 16，取决于平台。
2. **`reinterpret_cast<T*>(buf)` 不立即创建对象**。C++20 之前必须先 placement new，C++20 只对 trivially copyable 类型自动隐式创建。
3. **vptr 在构造函数中被多次写入**。每个基类的构造函数都会设置 vptr 指向自己的 vtable，最终在最派生类的构造函数中设置为最派生 vtable。
4. **`delete this` 是合法的**——但之后不得访问任何成员，且必须确保对象是 `new` 创建的。
5. **零大小 `new` 返回唯一非空指针**。`new char[0]` 不返回 `nullptr`，保证与任何其他 `new` 返回值不同。
6. **`std::launder` 不是万能的**。它只能"复活"同一地址的同类型对象，不能跨越类型界限，也不能使已释放内存上的指针合法。

## 延伸阅读

- [RAII 与资源管理](/topics/raii) — 构造/析构与资源管理的惯用法
- [值类别深度解析](/topics/value-categories-deep-dive) — 左值、右值、将亡值与移动语义
- [编译器优化全景](/topics/compiler-optimizations) — 去虚拟化、内联、别名分析如何利用对象模型
- [内存模型与并发](/topics/memory-model) — 对象在多线程中的可见性保证
- [模板元编程](/topics/template-metaprogramming) — `is_trivially_copyable` 等 traits 的实现原理
