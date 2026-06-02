---
title: 对象生命周期
topic: topics
feature: lifetime
status_checked_at: 2026-06-02
standard: N/A
---

# 对象生命周期

## 对象的诞生与消亡

C++ 标准定义：**对象是一段存储区域**（[intro.object]），有类型、地址和生命周期。生命周期从**初始化完成**时开始，到**销毁**或**存储被回收**时结束。

```cpp
int main() {
    int x = 42;              // 初始化完成 → 生命周期开始
    std::string s = "hello"; // 构造函数完成 → 生命周期开始
} // x 存储回收，s 析构函数调用 → 两者生命周期结束
```

**核心规则**（[basic.life]）：

```
生命周期开始：
  - class 类型：整个构造过程完成（不只是构造函数体）
  - 标量类型：初始化完成瞬间
  - 聚合体：每个成员初始化完成后

生命周期结束：
  - class 类型：析构函数被调用
  - 标量/POD：存储被回收或重用
```

### 析构函数的调用时机

```cpp
{
    std::string s = "hello";
    std::string t = std::move(s); // t 构造，s 合法但未指定状态
} // t 先析构，s 后析构（构造的逆序）
```

析构函数**不会**在以下情况被调用：对象通过 placement new 构造但未显式调用析构、程序通过 `std::exit()`/`std::abort()` 终止、栈展开中某析构函数抛异常触发 `std::terminate`。

### 构造函数中的部分构造

```cpp
class Widget {
    std::string name_;
    std::unique_ptr<int> data_;
public:
    Widget(std::string n)
        : name_(std::move(n))            // 先构造
        , data_(std::make_unique<int>(42)) // 后构造
    {
        // 若此处抛异常：name_ 已构造会被析构，data_ 未构造则不析构
        // 已构造的基类和成员按逆序析构
    }
};
```

## 四种存储期

### 自动存储期（automatic）

局部变量和函数参数分配在栈上，离开作用域时销毁。编译器保证逆序析构，包括异常导致的栈展开：

```cpp
void risky() {
    std::string a = "first";
    std::string b = "second";
    throw std::runtime_error("boom");
    // 编译器保证：b.~string() → a.~string() → 异常传播
}
```

**⚠️ 陷阱**——返回局部变量的引用/指针导致悬垂：

```cpp
int& bad() { int x = 42; return x; }      // ❌ 悬垂引用
std::string good() { std::string s = "h"; return s; } // ✅ NRVO / 移动
```

### 静态存储期（static）

全局变量、`static` 局部变量、`namespace` 作用域变量拥有静态存储期。程序启动时分配，终止时销毁。

```cpp
int global = 42;
void f() { static int counter = 0; ++counter; } // 首次调用时初始化
```

初始化分两个阶段：**零初始化**（程序启动，保证不是垃圾值）→ **动态初始化**（`main()` 之前）。跨翻译单元的动态初始化顺序是未定义的（**static initialization order fiasco**）。

```cpp
// ❌ 经典陷阱：跨翻译单元
// a.cpp: std::string a = "hello";
// b.cpp: extern std::string a; std::string b = a + " world"; // a 可能未初始化

// ✅ Meyer's Singleton（C++11 起局部 static 线程安全初始化）
std::string& get_a() { static std::string a = "hello"; return a; }
```

**`constinit`（C++20）**：强制常量初始化，避免动态初始化的顺序和线程安全问题：

```cpp
constinit int safe = 42;      // 编译期初始化，无运行时开销
constinit int mut = 0;
mut = 1;                       // ✅ constinit 不是 const
// constinit int bad = get_value(); // ❌ 不是常量表达式
```

### 线程局部存储期（thread_local）

`thread_local` 变量每线程独立存在，线程创建时初始化，线程销毁时析构：

```cpp
thread_local int tls_counter = 0; // 每个线程独立副本
void f() { static thread_local std::string s = "init"; } // per-thread lazy init
```

### 动态存储期（dynamic）

通过 `new`/`delete` 或分配器间接管理：

```cpp
auto p = new int(42);           // 生命周期开始
delete p;                       // 生命周期结束

alignas(std::string) char buf[sizeof(std::string)];
auto ps = new (buf) std::string("hello"); // placement new
ps->~basic_string();                      // 必须显式析构

// ⚠️ 常见错误：忘记 delete（泄漏）、双重释放（UB）、use-after-free（UB）
```

## 子对象的生命周期

对象可包含成员子对象、基类子对象、数组元素。构造按声明顺序，析构按逆序。

### 成员子对象

```cpp
class Container {
    std::vector<int> vec_;  // 先构造
    std::string name_;      // 后构造
public:
    Container() : vec_{1,2,3}, name_("c") {}
    // 析构顺序：name_ → vec_
};

// ⚠️ 初始化列表顺序由声明顺序决定
struct Trap {
    int a; int b;
    Trap() : b(42), a(b) {} // ❌ a 先初始化，b 尚未初始化
};
```

### 数组与派生类

```cpp
std::string arr[3]; // 构造: [0]→[1]→[2]  析构: [2]→[1]→[0]
// ⚠️ 对数组用 delete（而非 delete[]）是 UB

class Derived : public Base {
    std::string data_;
public:
    // 构造: Base → data_ → Derived 函数体
    // 析构: Derived 函数体 → data_ → Base
};
// ⚠️ 通过基类指针删除派生类对象需要 virtual ~Base()，否则 UB
```

## 访问生命周期之外的对象——未定义行为

对象生命周期之外的访问是 UB（[basic.life]）。编译器可假设这种情况不发生，并基于此进行激进优化：

```cpp
int* p = new int(42);
delete p;
// 编译器可以假设 *p 不再被合法访问
if (*p == 42) { /* 可能被整个优化掉 */ }
```

### std::launder——穿透生命周期边界

C++17 引入，允许在特定条件下合法访问"重生"的对象：

```cpp
struct X { const int n; };
alignas(X) unsigned char buf[sizeof(X)];
X* p1 = new (buf) X{42};
X* p2 = new (buf) X{100}; // 同一存储上构造新对象
// p1->n 可能被编译器缓存为 42（const 成员 + 同一地址）
int val = std::launder(p1)->n; // ✅ 100：告诉编译器重新从内存读取
```

### P0137R1——对象模型的核心修订

C++17 采纳：编译器可假定指针只在其指向对象的生命周期内使用。使得跨类型指针的优化更激进，也使通过 `reinterpret_cast` 绕过类型系统变为更明显的 UB。

## std::start_lifetime_as（C++23）

允许在现有存储上隐式开始新对象的生命周期，无需调用构造函数：

```cpp
#include <memory>
alignas(int) unsigned char buf[4];
*std::start_lifetime_as<int>(buf) = 42; // ✅ 隐式开始 int 生命周期

// 典型应用：从 I/O 缓冲区读取结构化数据
struct PacketHeader { uint32_t magic; uint16_t length; uint16_t checksum; };
void process(std::span<std::byte> raw) {
    auto* h = std::start_lifetime_as<PacketHeader>(raw.data());
    // 合法访问 h->magic 等成员（无需 memcpy）
}
```

**与 placement new 的区别**：`placement new` 执行构造函数，`start_lifetime_as` 仅告知编译器该存储中已有合法对象。仅适用于 implicit-lifetime types（标量、trivial class 等）。

## 隐式对象创建（C++20）

C++20 的 P0593R6 引入"隐式对象创建"概念，修复了 `malloc`/`memcpy` 背后隐藏的大量 UB。

### P0593R6 核心思想

```cpp
// C++20 之前：malloc 不创建对象，直接赋值是 UB
void* p = malloc(sizeof(int));
*static_cast<int*>(p) = 42;  // ❌ 不存在 int 对象

// C++20 之后：malloc 是"隐式创建对象的操作"
// 自动在分配的存储中创建 implicit-lifetime 类型的对象
*static_cast<int*>(p) = 42;  // ✅
```

### 隐式创建对象的操作

包括 `operator new`、`std::malloc`/`calloc`/`realloc`、`std::allocator::allocate`、`std::memcpy`/`memmove`、`std::start_lifetime_as`（C++23）。这些操作会在存储中隐式创建足够多的 implicit-lifetime 类型对象。

```cpp
// memcpy 现在隐式创建目标中的对象
alignas(int) unsigned char buf[sizeof(int)];
std::memcpy(buf, &src, sizeof(int));
int* p = std::launder(reinterpret_cast<int*>(buf)); // ✅ *p 合法

// FlexArray 风格的变长缓冲区
struct Flex { std::size_t len; };
void* raw = std::malloc(sizeof(Flex) + 100);
auto* f = static_cast<Flex*>(raw);
f->len = 100;
char* data = reinterpret_cast<char*>(f + 1);
// C++20: malloc 隐式创建了 Flex 和 100 个 char → data[0]..[99] 合法
```

## 指针与引用的生命周期规则

### 指针失效

```cpp
int* p;
{ int x = 42; p = &x; }
// x 销毁后，p 变为 invalid pointer value
// 解引用、比较、递增都是 UB（仅允许与 nullptr 比较或作为 delete 操作数）
```

### 引用的生命周期延长

`const T&` 或 `T&&` 绑定到临时对象时，临时对象生命周期延长到引用作用域结束。**但有关键例外**：

```cpp
const std::string& ref = std::string("temp"); // ✅ 延长到 ref 作用域结束

// ❌ 不延长的三种情况：
void f(const std::string& s);           // 函数参数引用——不延长
f(std::string("temp"));                 // 临时对象在完整表达式后销毁

const auto& r = std::pair{1,2}.first;   // 绑定子对象——不延长
const auto& bad = get_string();          // 可能绑定返回的临时——不延长
```

## 悬垂引用与指针

### 典型产生模式

```cpp
// 1. 返回局部变量的引用/指针
int* make() { int x = 42; return &x; } // ❌

// 2. 持有已销毁容器元素的引用
std::vector<int> v = {1,2,3};
int& ref = v[0];
v.push_back(4); // 可能重新分配 → ref 悬垂

// 3. lambda 捕获局部变量引用
auto make_lambda() {
    int x = 42;
    return [&x]() { return x; }; // ❌ x 在函数返回后销毁
}

// 4. unique_ptr 的 get() 借出后 reset
Holder h; h.data = std::make_unique<int>(42);
int& ref = *h.data;
h.data.reset(); // ref 悬垂
```

### 检测工具

- **编译器**：`-Wall -Wextra -Wdangling-reference`（GCC 13+）
- **运行时**：AddressSanitizer（`-fsanitize=address`）检测 use-after-free
- **静态分析**：Clang 的 `-Wlifetime`（实验性）

## 容器中的生命周期安全

### 迭代器失效

```cpp
std::vector<int> v = {1,2,3};
auto it = v.begin();
v.push_back(4); // 若触发重新分配 → it 悬垂

v.reserve(100);  // 预分配后 push_back 不重新分配
auto it2 = v.begin();
v.push_back(4);  // it2 仍有效 ✅
```

**各容器的迭代器稳定性**：

| 容器 | 插入 | 删除 |
|------|------|------|
| `vector` | 全部失效（重新分配时） | 被删及之后失效 |
| `deque` | 中间插入：全部失效；两端插入：全部失效 | 中间删除：全部失效；两端删除：仅迭代器失效 |
| `list` | 不失效 ✅ | 仅被删元素失效 |
| `unordered_map` | 可能 rehash → 全部迭代器失效，但引用稳定 | 仅被删元素失效 |

### span / ranges::view 的生命周期

```cpp
// span 不拥有数据，底层数据必须存活
std::span<int> make_span() {
    std::vector<int> v = {1,2,3};
    return std::span<int>(v); // ❌ v 返回后销毁，span 悬垂
}
// ranges 视图同理：v | std::views::filter(...) 不延长 v 的生命周期
```

## RAII 与生命周期管理

RAII 的核心：资源生命周期绑定到对象生命周期——构造函数获取，析构函数释放。编译器保证局部对象在离开作用域时析构（包括栈展开），因此 RAII 系统性地避免了手写释放路径导致的泄漏。

```cpp
// ❌ 手动管理——每个退出点都可能泄漏
void raw(const char* path) {
    FILE* f = fopen(path, "r"); if (!f) return;
    char* buf = (char*)malloc(4096); if (!buf) { fclose(f); return; }
    // ... 更多退出点
    fclose(f); free(buf);
}

// ✅ RAII——析构函数保证释放
void safe(const char* path) {
    auto f = std::unique_ptr<FILE, decltype(&fclose)>(fopen(path, "r"), &fclose);
    auto buf = std::make_unique<char[]>(4096);
    // 任意复杂的控制流，包括抛异常——析构函数兜底
}
```

**析构顺序保证**：逆序销毁。栈展开期间编译器为每个已构造的局部对象调用析构函数，因此中间抛异常也能正确释放。但析构函数本身不能抛异常——会导致 `std::terminate`。

详见 [RAII 与资源管理](/topics/raii)。

## 智能指针的生命周期语义

### unique_ptr

独占所有权，零额外开销（默认删除器时大小等于裸指针）：

```cpp
auto p = std::make_unique<Widget>(42);
auto p2 = std::move(p);  // 所有权转移，p 变为 nullptr
Widget* raw = p2.get();   // 借出裸指针——raw 的有效性依赖 p2 的生命周期
```

### shared_ptr 与引用计数

共享所有权，最后一个 `shared_ptr` 销毁时对象被销毁：

```cpp
auto sp1 = std::make_shared<Widget>(); // 控制块 + Widget 合并分配（一次 malloc）
auto sp2 = sp1;  // 引用计数 = 2
sp1.reset();      // 引用计数 = 1
sp2.reset();      // 引用计数 = 0 → Widget 销毁

// ⚠️ 原子引用计数有性能开销——高频路径考虑 unique_ptr 或值语义
```

### weak_ptr——打破循环引用

```cpp
struct Node {
    std::vector<std::shared_ptr<Node>> children;
    std::weak_ptr<Node> parent; // ✅ 打破父子循环
};

auto root = std::make_shared<Node>();
auto child = std::make_shared<Node>();
root->children.push_back(child);
child->parent = root; // weak_ptr 不增加引用计数

if (auto p = child->parent.lock()) { /* parent 存活 */ }
```

### enable_shared_from_this

在对象回调中需要持有自身的 `shared_ptr` 时使用——避免创建多个独立的引用计数控制块：

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    void start() {
        auto self = shared_from_this(); // ✅ 复用已有控制块
        async_read([self](auto...) { self->on_data(); });
    }
};
// ⚠️ 前提：必须已有 shared_ptr 管理该对象。裸对象上调用是 UB。
```

## constexpr 生命周期求值

`constexpr`/`consteval` 函数中的对象生命周期遵循编译期特殊规则。

```cpp
constexpr int compute() {
    int a = 1, b = 2; // 编译期自动存储期——生命周期在 compute() 求值期间
    return a + b;
}

// C++20: constexpr 容器（返回值通过移动转移）
constexpr auto make_array() {
    std::array<int,3> arr = {1,2,3};
    return arr; // 编译期移动语义
}
constexpr auto a = make_array();

// C++20: constexpr 动态分配——编译期不泄漏
constexpr int test() {
    auto p = std::make_unique<int>(42); // C++23 constexpr unique_ptr
    return *p;
} // 编译器保证 p 在常量求值结束时释放

// ⚠️ constexpr 不能返回局部变量的指针（即使在编译期，生命周期规则也不允许悬垂）
// ✅ 字面量和全局变量拥有静态存储期，可以安全从 constexpr 返回
constexpr const char* hello() { return "hello"; }
```

### consteval——强制编译期

```cpp
consteval int must_compile(int n) {
    int result = 0;
    for (int i = 0; i < n; ++i) result += i;
    return result;
}
static_assert(must_compile(10) == 45);
// consteval 中的对象生命周期完全在编译期，编译器保证求值结束后所有资源正确释放

// C++20: constexpr 虚函数
struct Base { constexpr virtual int value() const { return 0; } };
struct Derived : Base { constexpr int value() const override { return 42; } };
constexpr int v = Derived{}.value(); // ✅
```

## 最佳实践清单

```
1. 优先值语义和 RAII 容器，避免手动 new/delete
2. 返回局部变量时用值返回（不要返回引用/指针）
3. push_back/insert 后不复用旧迭代器
4. shared_ptr 循环引用用 weak_ptr 打破
5. enable_shared_from_this 替代"两个独立 shared_ptr 管理同一裸指针"
6. span/string_view/view 非拥有——底层数据必须存活
7. placement new 后必须显式析构（除非 implicit-lifetime type 用 start_lifetime_as）
8. 使能 -Wall -Wextra -Wdangling-reference，CI 运行 ASan/UBSan
9. constinit 替代 namespace-scope 非 constexpr 变量的动态初始化
10. 避免 reinterpret_cast 到不相关类型——违反 strict aliasing 是 UB
```

## 延伸阅读

- [RAII 与资源管理](/topics/raii) — 构造/析构、智能指针、scope guard
- [内存模型与并发](/topics/memory-model) — 对象存储与多线程可见性
- [编译期计算](/topics/compile-time-computation) — constexpr/consteval/constinit
- [性能优化](/topics/performance) — 值语义、移动语义的性能影响
- [C++ Core Guidelines Lifetime](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#pro-lifetime-safety) — 静态分析规则
- P0593R6 — Implicit creation of objects for low-level object manipulation
- P0137R1 — Core Issue 1776: Replacement of class objects containing reference members
