---
title: "RTTI 与虚函数表实现机制"
topic: topics
feature: rtti-vtable
status_checked_at: 2026-06-02
standard: N/A
---

# RTTI 与虚函数表实现机制

> C++ 的多态并非魔法——它是一套由编译器自动生成的函数指针表（vtable）和类型元数据（type_info）构成的精确定向机制。理解这套机制是写好 C++ 的必经之路。

---

## 一、虚函数机制

### 1.1 从源码到机器码

```cpp
class Base {
public:
    virtual void speak() { std::cout << "Base\n"; }
    virtual void walk()  { std::cout << "Base walk\n"; }
    virtual ~Base() = default;
};

class Derived : public Base {
public:
    void speak() override { std::cout << "Derived\n"; }
    // walk() 继承自 Base
    ~Derived() override = default;
};

void polymorphic_call(Base* p) {
    p->speak();  // 虚调用：运行时确定目标
}
```

虚调用的实际执行过程：

```
polymorphic_call(Base* p) 编译后的伪代码：

    // 1. 从对象首地址取出 vptr（指向 vtable 的指针）
    vptr = *(void***)p;

    // 2. 用 vptr 索引 vtable，取出第 0 项（speak 的函数指针）
    fn_ptr = vptr[0];   // speak() 在 vtable 中的偏移

    // 3. 通过函数指针间接调用，将 p 作为隐式 this 传入
    fn_ptr(p);
```

### 1.2 vptr 的位置

vptr 在对象内存布局中的位置（Itanium ABI）：

```
┌────────────────────────────────────┐
│ vptr                               │ ← 对象起始地址（偏移 0）
├────────────────────────────────────┤
│ Base::member1                      │
├────────────────────────────────────┤
│ Base::member2                      │
├────────────────────────────────────┤
│ Derived::member3                   │
└────────────────────────────────────┘

Itanium ABI 将 vptr 放在对象起始地址（偏移 0）。
MSVC 也采用相同策略。
```

vptr 的初始化发生在构造函数中——编译器在每个构造函数体前插入 vptr 赋值代码：

```
Base::Base() 编译后的伪代码：

    p->vptr = &vtable_for_Base;   // 编译器插入
    // 用户构造函数体
    ...

Derived::Derived() 编译后的伪代码：

    Base::Base(p);                // 先调用基类构造（其中 vptr = &vtable_for_Base）
    p->vptr = &vtable_for_Derived; // 编译器插入：覆盖为派生类 vtable
    // 用户构造函数体
    ...
```

---

## 二、vtable 布局（Itanium C++ ABI）

### 2.1 单继承下的 vtable 布局

Itanium C++ ABI 是 GCC/Clang 在类 Unix 平台上的事实标准。

```cpp
class A {
public:
    virtual void f1();
    virtual void f2();
    virtual void f3();
    ~A();  // virtual (默认析构)
};

class B : public A {
public:
    void f1() override;    // 覆盖
    virtual void f4();     // 新增
    ~B() override;
};
```

```
类 A 的 vtable：

┌──────────────────────────────┬──────────────────────┐
│ 索引 │ 内容                  │ 说明                  │
├──────┼───────────────────────┼──────────────────────┤
│  -3  │ offset_to_top        │ 0（到主基类的偏移）   │
│  -2  │ RTTI pointer         │ typeinfo_for_A        │
│  -1  │ ...                  │ （ABI 预留槽位）       │
│   0  │ A::f1()              │ 第一个虚函数          │
│   1  │ A::f2()              │                       │
│   2  │ A::f3()              │                       │
│   3  │ A::~A() (deleting)   │ 虚析构（删除型）      │
│   4  │ A::~A() (complete)   │ 虚析构（完整型）      │
└──────┴──────────────────────┴──────────────────────┘

类 B 的 vtable：

┌──────┬───────────────────────┬──────────────────────┐
│ 索引 │ 内容                  │ 说明                  │
├──────┼───────────────────────┼──────────────────────┤
│  -3  │ offset_to_top        │ 0（单继承，无偏移）   │
│  -2  │ RTTI pointer         │ typeinfo_for_B        │
│  -1  │ ...                  │                       │
│   0  │ B::f1()              │ 覆盖了 A::f1          │
│   1  │ A::f2()              │ 继承                  │
│   2  │ A::f3()              │ 继承                  │
│   3  │ B::~B() (deleting)   │ 覆盖析构              │
│   4  │ B::~B() (complete)   │                       │
│   5  │ B::f4()              │ 追加在末尾            │
└──────┴───────────────────────┴──────────────────────┘

关键规则：
  · 虚函数按声明顺序排列
  · override 覆盖对应槽位，新增虚函数追加到末尾
  · vtable 从偏移 0 开始是虚函数，负偏移是 RTTI 和辅助信息
```

### 2.2 虚析构函数的两个槽位

Itanium ABI 为虚析构函数保留两个槽位：

```
deleting destructor（删除型）：
    调用完整析构后，释放对象内存（operator delete）
    用于 delete p; 场景

complete destructor（完整型）：
    调用析构函数体，但不释放内存
    用于栈上对象析构、placement delete 等场景

子对象析构（base destructor）：
    仅析构基类和成员，不涉及派生类部分
    由 derived destructor 调用，不出现在 vtable 中
```

---

## 三、多重继承下的 vtable 布局

多重继承引入了两个关键问题：一个对象需要多个 vptr（每个基类一个），以及 `this` 指针调整。

```cpp
class A {
public:
    virtual void f();
    int a;
};

class B {
public:
    virtual void g();
    int b;
};

class C : public A, public B {
public:
    void f() override;
    void g() override;
    int c;
};
```

```
对象 C 的内存布局：

┌──────────────────────────────────┐  ← this 指向此处（C*）
│ vptr_for_A（主基类 vptr）         │
├──────────────────────────────────┤
│ A::a                             │
├──────────────────────────────────┤  ← (char*)this + offset_to_B
│ vptr_for_B（辅助基类 vptr）       │
├──────────────────────────────────┤
│ B::b                             │
├──────────────────────────────────┤
│ C::c                             │
└──────────────────────────────────┘

C 的主 vtable（vptr_for_A 指向此处）：

┌──────┬───────────────────────────┐
│  -3  │ offset_to_top = 0         │  ← 主基类，偏移为 0
│  -2  │ RTTI pointer for C        │
│   0  │ C::f()                    │  覆盖 A::f
│   1  │ C::g()                    │  覆盖 B::g（带 thunk 调整 this）
└──────┴───────────────────────────┘

C 的辅助 vtable（vptr_for_B 指向此处）：

┌──────┬───────────────────────────┐
│  -3  │ offset_to_top = sizeof(A) │  ← 指回 C 的起始地址
│  -2  │ RTTI pointer for C        │
│   0  │ C::g()                    │  覆盖 B::g（thunk 调整 this）
└──────┴───────────────────────────┘
```

### 3.1 Thunk：this 指针调整

当通过 `B*` 指针调用 `C::g()` 时，`this` 指向 B 子对象（不是 C 的起始地址）。但 `C::g()` 需要的是 C 的 `this`，因此编译器生成一个 thunk：

```cpp
// 伪代码：C::g() 的 thunk
void C::g() [thunk for B] {
    this = this - sizeof(A);  // 将 this 从 B* 调整回 C*
    return C::g();            // 跳转到真正的实现
}
```

thunk 只做两件事：调整 `this`，然后跳转到实际函数。编译器把 thunk 的地址放进辅助 vtable，调用方无需关心。

---

## 四、虚继承与 VTT

### 4.1 虚继承的菱形问题

```cpp
class A { public: virtual void f(); int a; };

class B : public virtual A {
public:
    virtual void g();
    void f() override;
    int b;
};

class C : public virtual A {
public:
    virtual void h();
    void f() override;
    int c;
};

class D : public B, public C {
public:
    void f() override;
    int d;
};
```

```
菱形继承的对象布局（D 的实例）：

┌──────────────────────────────────┐  ← D* / B* 指向这里
│ vptr_for_B                       │
├──────────────────────────────────┤
│ B::b                             │
├──────────────────────────────────┤  ← C* 指向这里
│ vptr_for_C                       │
├──────────────────────────────────┤
│ C::c                             │
├──────────────────────────────────┤
│ D::d                             │
├──────────────────────────────────┤  ← A* 指向这里（共享副本）
│ vptr_for_A                       │
├──────────────────────────────────┤
│ A::a                             │
└──────────────────────────────────┘

A 子对象只有一份——这就是虚继承的意义。
每个基类子对象通过 vtable 中的 offset 字段定位共享的 A。
```

### 4.2 VTT（Virtual Table Table）

虚继承的 vtable 不能静态确定——派生类构造函数必须动态调整基类 vtable 中的偏移值。为此，ABI 引入了 VTT（Virtual Table Table），一个指向所有相关 vtable 的指针数组。

```
VTT 结构（以 D 为例）：

┌────────────────────────────────────────────────────────────┐
│ VTT for D                                                  │
├────────────────────────────────────────────────────────────┤
│ [0] → D 的主 vtable（完整版，所有偏移已确定）              │
│ [1] → D 的主 vtable 中 B 子对象的 vtable 部分             │
│ [2] → D 的主 vtable 中 C 子对象的 vtable 部分             │
│ [3] → B::construction vtable（用于构造 B 子对象时）       │
│ [4] → B::construction vtable 中 A 的部分                  │
│ [5] → C::construction vtable（用于构造 C 子对象时）       │
│ [6] → C::construction vtable 中 A 的部分                  │
└────────────────────────────────────────────────────────────┘

构造 D 对象时，编译器生成的伪代码：

D::D(D* this) {
    // 构造 B 子对象：先用 construction vtable（A 的偏移未定）
    this->B_vptr = VTT[3];  // 指向 B 的 construction vtable
    B::B();
    // 构造 C 子对象
    this->C_vptr = VTT[5];  // 指向 C 的 construction vtable
    C::C();
    // 共享的 A 子对象：用完整的 A vtable
    this->A_vptr = &vtable_for_A;
    A::A();
    // 最后：替换为 D 的完整 vtable（此时所有偏移已确定）
    this->B_vptr = VTT[1];  // D 的主 vtable 中 B 的部分
    this->C_vptr = VTT[2];  // D 的主 vtable 中 C 的部分
    // D 自身构造函数体
}
```

VTT 存在于每个参与虚继承层次的类中，用于构造/析构期间正确设置 vptr。对象完全构造后，vptr 指向稳定的"完整" vtable。

---

## 五、纯虚函数与 `__cxa_pure_virtual`

### 5.1 纯虚函数在 vtable 中的表示

```cpp
class Abstract {
public:
    virtual void concrete() { /* ... */ }
    virtual void must_implement() = 0;  // 纯虚函数
};
```

```
Abstract 的 vtable：

┌──────┬──────────────────────────────┐
│  -3  │ offset_to_top = 0            │
│  -2  │ RTTI pointer                 │
│   0  │ Abstract::concrete()         │
│   1  │ __cxa_pure_virtual           │  ← 指向运行时终止函数
└──────┴──────────────────────────────┘

如果运行时错误地调用纯虚函数：

__cxa_pure_virtual() 的实现（libcxxabi 伪代码）：

    extern "C" void __cxa_pure_virtual() {
        // 在 stderr 输出错误信息，然后终止程序
        abort_message("Pure virtual function called!");
        // 通常输出类似：
        // "pure virtual method called\n"
        // terminate() — 最终调用 std::abort()
    }

触发场景：
  · 在基类构造函数中调用纯虚函数（此时 vptr 仍指向基类 vtable）
  · 在基类析构函数中调用纯虚函数（vptr 已回退到基类）
  · 通过未定义行为（悬挂指针）误调
```

---

## 六、RTTI 实现：type_info 对象

### 6.1 type_info 的结构

```cpp
// Itanium ABI 中 type_info 的内部结构（简化）
class type_info {
    // ABI 要求的字段
    void* __typeinfo_name;  // mangled name 的指针（延迟计算）

    // 编译器扩展字段（因实现而异）
    const char* __name;

    // 虚函数（实现继承层次的比较）
    virtual ~type_info();
    virtual bool __is_pointer_p() const;
    virtual bool __is_function_p() const;
    virtual bool __do_catch(const type_info* thrown_type,
                            void** thrown_object,
                            unsigned outer) const;
    virtual bool __do_upcast(const __cxx_class_info* target,
                             void** obj_ptr) const;
};

// 每个类一个 type_info 对象（在 .rodata 段中静态分配）
// 编译器为每个参与多态的类生成 typeinfo_for_ClassName
```

```
type_info 的内存布局：

┌──────────────────────────────────────┐
│ vptr（type_info 自身的 vtable）       │  ← type_info 也有虚函数
├──────────────────────────────────────┤
│ __typeinfo_name (void*)              │  → mangled name（延迟获取）
├──────────────────────────────────────┤
│ __name (const char*)                 │  → demangled name（用于 name()）
└──────────────────────────────────────┘

存储位置：
  · type_info 对象本身在 .rodata 段（只读数据）
  · mangled name 字符串也在 .rodata 段
  · 每个翻译单元中每种类型最多一个 type_info 实例
  · 链接器通过弱符号合并同一类型在不同 .o 中的 type_info
```

### 6.2 name() 与比较

```cpp
#include <typeinfo>
#include <iostream>

struct Widget {};
struct Gadget : Widget {};

void demo() {
    const std::type_info& t1 = typeid(Widget);
    const std::type_info& t2 = typeid(Gadget);

    // name() 返回实现定义的 mangled name
    std::cout << t1.name() << "\n";  // 6Widget（GCC）| struct Widget（MSVC）
    std::cout << t2.name() << "\n";

    // 比较操作
    // before()：用于排序，实现 defines total ordering
    bool ordered = t1.before(t2);

    // ==：判断是否为同一类型（指针比较，O(1)）
    bool same = (t1 == t2);  // false

    // hash_code()：C++11，用于 unordered 容器
    size_t h = t1.hash_code();
}
```

---

## 七、`dynamic_cast` 实现

### 7.1 转换类型

```cpp
class Base { public: virtual ~Base() = default; };
class Derived : public Base { int x; };
class Unrelated { int y; };

Base* bp = new Derived;

// 1. downcast（向下转换）—— type_info 比较 + 偏移调整
Derived* dp = dynamic_cast<Derived*>(bp);  // 成功 → 返回调整后的指针

// 2. crosscast（交叉转换）—— 遍历类型层次
class Left  { public: virtual ~Left() = default; };
class Right { public: virtual ~Right() = default; };
class Diamond : public Left, public Right { public: ~Diamond() override = default; };
Left* lp = new Diamond;
Right* rp = dynamic_cast<Right*>(lp);  // 成功 → Diamond → Right 的偏移

// 3. 失败的转换
Unrelated* up = dynamic_cast<Unrelated*>(bp);  // 返回 nullptr（指针版本）
// 引用版本会抛 std::bad_cast
```

### 7.2 运行时算法

```
dynamic_cast<TargetType*>(src_ptr) 的实现步骤：

  1. 若 src_ptr == nullptr → 返回 nullptr（空指针转换恒为空）

  2. 从 src_ptr 的 vtable 中取出 RTTI pointer
     → 获取 src_type = *RTTI_pointer

  3. 比较 src_type 与 target_type：
     a. 若 src_type == target_type（同一 type_info）→ 直接返回
     b. 若 是目标类型的子类 → 向上调整偏移，返回
     c. 若 是目标类型的父类 → 向下检查，失败则返回 nullptr

  4. 交叉转换（crosscast）：
     · 遍历完整类型层次图（DFS/BFS）
     · 检查每个中间类型的 type_info 与 target_type 匹配
     · Itanium ABI 使用 __class_type_info 层次结构
     · 此路径较慢——可能需要遍历整个继承树

  5. 若找不到匹配 → 返回 nullptr（指针）或抛 bad_cast（引用）

性能特征：
  · 同一类型转换：O(1) —— 一次 type_info 指针比较
  · 单继承 downcast：O(depth) —— depth 为继承层次深度
  · 交叉转换：O(N) —— N 为继承树中的类数量
```

---

## 八、`typeid` 实现

```cpp
// typeid 的两种用法
void demo(Base* p) {
    // 1. 静态类型已知（编译期）
    const std::type_info& t1 = typeid(int);     // 直接返回静态 type_info
    const std::type_info& t2 = typeid(Base);     // 不解引用指针

    // 2. 多态类型（运行时 —— 通过 vtable 获取）
    const std::type_info& t3 = typeid(*p);       // 解引用多态指针
    // 实现：p->vptr[-2] → RTTI pointer → type_info 对象
}
```

```
typeid(*p) 的编译后代码：

    // p 指向多态对象
    vptr = *(void***)p;        // 取 vptr
    rtti_ptr = vptr[-2];       // RTTI pointer 在 vtable 偏移 -2 处
    return *rtti_ptr;          // 返回 type_info 引用

注意事项：
  · typeid(int) 等内置类型在编译期解析，无运行时开销
  · typeid(非多态类型) 不需要 vtable，编译期确定
  · typeid(*p) 仅当 p 的静态类型有多态性时才执行运行时查询
  · 若 p 为 null → std::bad_typeid 异常
```

---

## 九、`-fno-rtti` 的影响

### 9.1 禁用 RTTI 后的行为

```
编译选项 -fno-rtti 的效果：

  ┌─────────────────────────────────────────────────────────────┐
  │ 被禁用的功能                                                │
  ├─────────────────────────────────────────────────────────────┤
  │ · typeid 对多态类型的操作 → 编译错误                        │
  │ · dynamic_cast → 编译错误                                   │
  │ · type_info 类 → 不可用                                     │
  │ · vtable 中的 RTTI 槽位 → 不生成（二进制体积减小）          │
  ├─────────────────────────────────────────────────────────────┤
  │ 保留的功能                                                   │
  ├─────────────────────────────────────────────────────────────┤
  │ · virtual 函数调用 → 正常工作（vtable 仍在）                │
  │ · typeid(int) 等内置类型 → 编译期解析，不受影响              │
  │ · 非多态类的 typeid → 编译期解析，不受影响                   │
  └─────────────────────────────────────────────────────────────┘

典型使用场景：
  · 嵌入式系统（减小二进制体积）
  · 游戏引擎（确定不需要 dynamic_cast 时，消除 RTTI 元数据）
  · LLVM/Clang 项目本身使用 -fno-rtti + LLVM 自己的类型系统
```

### 9.2 `-fno-rtti` 下的替代方案

```cpp
// 方案一：枚举类型 ID
class Shape {
public:
    enum class Kind { Circle, Square, Triangle };
    virtual Kind kind() const = 0;  // 比 dynamic_cast 快得多
};

class Circle : public Shape {
    Kind kind() const override { return Kind::Circle; }
};

// 用法
void draw(Shape* s) {
    switch (s->kind()) {  // 直接调用虚函数，无 type_info 比较
    case Shape::Kind::Circle:  draw_circle(static_cast<Circle*>(s)); break;
    case Shape::Kind::Square:  draw_square(static_cast<Square*>(s)); break;
    }
}

// 方案二：LLVM 风格的 RTTI（classof + isa/cast/dyn_cast）
// 利用枚举 ID 进行类型检查，用 static_cast 替代 dynamic_cast
// https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
```

---

## 十、RTTI 的开销模型

```
空间开销：
  ┌───────────────────────────────────────────────────────┐
  │ 每个多态类：                                          │
  │   · 1 个 type_info 对象（约 16-32 字节，指针+名称）  │
  │   · 1 个 mangled name 字符串（长度因类名而异）       │
  │   · vtable 中 1 个 RTTI 指针槽位（8 字节）           │
  │                                                       │
  │ 总计：每个类约 50-100 字节（含名称）                  │
  │ 1000 个多态类 ≈ 50-100 KB                             │
  └───────────────────────────────────────────────────────┘

时间开销：
  ┌───────────────────────────────────────────────────────┐
  │ 操作                   │ 开销                         │
  ├────────────────────────┼──────────────────────────────┤
  │ typeid(*p)             │ 1 次 vtable 读取 + 1 次指针 │
  │                        │ 解引用（~几纳秒）            │
  ├────────────────────────┼──────────────────────────────┤
  │ dynamic_cast<T*>(p)    │ 同类型 O(1)，层次遍历       │
  │ (同类型)               │ O(depth)，交叉转换 O(N)     │
  ├────────────────────────┼──────────────────────────────┤
  │ dynamic_cast<T*>(p)    │ type_info::before() 比较     │
  │ (crosscast)            │ 遍历继承图 — 较慢            │
  ├────────────────────────┼──────────────────────────────┤
  │ type_info::name()      │ 仅返回指针（零开销）         │
  ├────────────────────────┼──────────────────────────────┤
  │ type_info == 比较       │ 指针比较 O(1)               │
  └────────────────────────┴──────────────────────────────┘

对比虚函数调用本身的开销：
  · 虚调用：1 次 vptr 读取 + 1 次函数指针读取 + 间接跳转
  · RTTI 查询（typeid）：额外 1 次指针解引用
  · dynamic_cast：显著高于 typeid —— 尤其是交叉转换
```

---

## 十一、去虚拟化（Devirtualization）

编译器在可能的情况下将虚调用替换为直接调用，消除间接跳销开销并允许内联。

### 11.1 编译器可自动去虚拟化的场景

```cpp
// 场景 1：已知具体类型
Derived d;
Base& b = d;
b.speak();  // 编译器知道 b 绑定到 Derived → 直接调用 Derived::speak()

// 场景 2：final 类
class Final final : public Base {
    void speak() override final;  // 无子类可覆盖
};

void call(Final& f) {
    f.speak();  // 编译器直接调用 Final::speak()

// 场景 3：final 虚函数
class Base2 {
    virtual void done() final;  // 不能再被覆盖
};

// 场景 4：LTO（链接时优化）— 跨翻译单元可见全部层次结构
// 编译器可以在链接时确定所有覆盖关系
```

### 11.2 LLVM 的去虚拟化实现

```
LLVM 的去虚拟化 passes：

  1. -fstrict-vtable-pointers（标注 vptr 不变性的假设）
     在对象构造完成后到析构开始前，vptr 不变。
     LLVM 插入 llvm.assume 来标注这一点。

  2. GlobalDPass（IPO pass）
     · 收集所有虚函数覆盖关系
     · 若某个虚函数只有一个可能的实现 → 替换为直接调用
     · 结合 LTO 效果最佳

  3. 标注（attributes）：
     · __attribute__((annotate("vtable_visibility", "all")))
       告诉编译器已知全部子类
     · [[clang::noescape]] 等属性辅助分析

手动去虚拟化技巧：
    auto& d = dynamic_cast<Derived&>(b);  // 转型后编译器知道具体类型
    d.speak();  // 可以直接调用

提示编译器：
    __builtin_assume(dynamic_cast<Derived*>(p) != nullptr);
    p->speak();  // 一些编译器能利用这个假设去虚拟化
```

---

## 十二、CRTP：编译期多态替代方案

CRTP（Curiously Recurring Template Pattern）在编译期实现静态多态，完全消除虚函数开销。

```cpp
// CRTP 基类
template <typename Derived>
class ShapeBase {
public:
    void draw() const {
        // 编译期派发：调用 Derived 的 draw_impl
        static_cast<const Derived*>(this)->draw_impl();
    }

    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

// 派生类
class Circle : public ShapeBase<Circle> {
    friend class ShapeBase<Circle>;  // 允许基类访问私有实现
    double radius_;
    void draw_impl() const { /* 画圆 */ }
    double area_impl() const { return 3.14159265 * radius_ * radius_; }
};

class Square : public ShapeBase<Square> {
    friend class ShapeBase<Square>;
    double side_;
    void draw_impl() const { /* 画方 */ }
    double area_impl() const { return side_ * side_; }
};

// 使用（无法存入同构容器 —— 类型不同）
Circle c;
c.draw();        // 编译器直接调用 Circle::draw_impl()
                 // 完全内联，零间接调用开销
```

### 12.1 CRTP vs 虚函数对比

```
┌──────────────────────┬───────────────────┬──────────────────────┐
│ 特性                  │ 虚函数             │ CRTP                 │
├──────────────────────┼───────────────────┼──────────────────────┤
│ 派发方式              │ 运行时（vtable）   │ 编译期（模板实例化） │
│ 间接调用开销          │ 有（每次调用）     │ 无（内联展开）       │
│ 代码体积              │ 共享              │ 每种类型一份实例     │
│ 同构容器              │ 可以（Base*）      │ 不可（类型不同）     │
│ 运行时多态            │ 支持              │ 不支持               │
│ 内联潜力              │ 差（间接跳转）     │ 优秀（静态派发）     │
│ 编译时间              │ 快                │ 模板膨胀 → 较慢     │
│ 调试体验              │ 直观              │ 模板错误信息难读     │
└──────────────────────┴───────────────────┴──────────────────────┘

选择建议：
  · 需要运行时多态（基类指针/引用、工厂模式）→ 虚函数
  · 性能关键路径、类型集固定、无需异构容器 → CRTP
  · std::visit + std::variant → 也提供编译期多态
```

---

## 十三、虚析构函数：必要性与实现

### 13.1 为什么需要虚析构函数

```cpp
class Base {
public:
    ~Base() { /* 非虚 */ }  // 危险！
};

class Derived : public Base {
    std::vector<int> data_;
public:
    ~Derived() { /* 释放 data_ */ }
};

Base* p = new Derived;
delete p;  // UB：通过非虚析构的基类指针删除派生类对象
           // 只调用 ~Base()，不调用 ~Derived()
           // → data_ 泄漏，且 ~Derived() 中可能有关键资源释放
```

```
虚析构函数的工作原理：

delete p 的编译后代码：

    // p 的静态类型是 Base*，Base 有虚析构
    vptr = *(void***)p;
    fn_ptr = vptr[3];  // deleting destructor 在 vtable 槽位 3

    fn_ptr(p);
    // 调用 Derived 的 deleting destructor：
    //   1. 调用 ~Derived()    ← 派生类析构
    //   2. 调用 ~Base()       ← 基类析构
    //   3. operator delete(p) ← 释放内存

经验法则：
  · 任何类只要有一个 virtual 函数 → 必须有 virtual 析构函数
  · 任何类不被用作基类（如 std::string_view）→ 不需要
  · C++ Core Guidelines C.35：基类的析构函数应为 public virtual
    或 protected non-virtual
```

### 13.2 `protected non-virtual` 析构的替代方案

```cpp
// 当不想支持基类指针删除时，使用 protected non-virtual 析构
class Interface {
protected:
    ~Interface() = default;  // 外部无法 delete Interface*
public:
    virtual void process() = 0;
};

class Impl : public Interface {
public:
    void process() override { /* ... */ }
    ~Impl() = default;       // 只能 delete Impl*
};

// Interface* p = new Impl;
// delete p;        // 编译错误：~Interface() is protected
// Impl* q = new Impl;
// delete q;        // 正确
```

---

## 十四、实现细节与平台差异

### 14.1 Itanium ABI vs MSVC ABI

```
┌────────────────────────┬──────────────────────┬────────────────────────┐
│ 特性                    │ Itanium ABI          │ MSVC ABI               │
│                        │ (GCC/Clang/Linux)    │ (MSVC/Windows)         │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ vtable 中 RTTI 的位置  │ 负偏移（offset -2）  │ vtable 第一个槽位      │
│                        │                      │ （正偏移 0）           │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ vtable 中函数起始位置  │ 偏移 0               │ 偏移 1（跳过 RTTI）    │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ offset_to_top          │ vtable 负偏移 -3     │ 不存在                 │
│                        │ 辅助 vtable 使用     │ 用 RTTI 内嵌的偏移     │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ 虚析构函数槽位数       │ 2（deleting+complete）│ 1（scalar/vector）     │
│                        │                      │ + 分离型析构           │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ RTTI 实现              │ type_info + 派生类   │ 完整的类层次结构       │
│                        │ __class_type_info    │ RTTIClassHierarchy     │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ VTT（虚继承 vtable 表）│ 有                   │ 无（用不同的初始化机制）│
├────────────────────────┼──────────────────────┼────────────────────────┤
│ type_info name()       │ 返回 mangled name    │ 返回 demangled name    │
└────────────────────────┴──────────────────────┴────────────────────────┘
```

### 14.2 MSVC 的 RTTI 结构

```
MSVC 的 Complete Object Locator (COL)：
  vtable[0] = RTTI Complete Object Locator
  vtable[1] = 第一个虚函数
  ...

COL 包含：
  · signature（标识符）
  · offset（对象起始到 vtable 的偏移）
  · cdOffset（构造位移表偏移）
  · type_descriptor（类的 type_info）
  · class_hierarchy_descriptor（类层次描述）

MSVC 的 ClassHierarchyDescriptor：
  · signature
  · attributes（是否多重继承等）
  · num_base_classes
  · base_class_array[] → 指向每个基类的 BaseClassDescriptor

这比 Itanium ABI 更显式——MSVC 在 RTTI 中存储了完整的类层次图，
用于 dynamic_cast 的层次遍历。
```

---

## 十五、最佳实践总结

```
┌────────────────────────────────────────────────────────────────────┐
│ 设计决策                                                          │
├────────────────────────────────────────────────────────────────────┤
│ 1. 有多态需求 → virtual 函数 + virtual 析构                       │
│ 2. 性能关键且类型固定 → CRTP 或 std::variant                      │
│ 3. 频繁 dynamic_cast → 考虑重构（虚函数分发、visitor 模式）       │
│ 4. 确定不需要 RTTI → -fno-rtti（节省空间，用枚举 ID 替代）       │
│ 5. 不需要继承删除 → protected non-virtual 析构                    │
│ 6. final 类/函数 → 帮助编译器去虚拟化                            │
├────────────────────────────────────────────────────────────────────┤
│ 性能指南                                                          │
├────────────────────────────────────────────────────────────────────┤
│ · 虚调用本身开销极小（~几纳秒），不是性能瓶颈                     │
│ · 真正的性能问题是：虚函数阻碍内联 → 放大调用链的累积开销         │
│ · dynamic_cast 交叉转换很慢 → 避免在热路径中使用                  │
│ · 虚函数的内存布局不利于 cache → 考虑 SoA / 扁平化设计           │
│ · 将虚函数放在调用链的最外层，内部使用非虚函数                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## 延伸阅读

- [编译器优化全景](/topics/compiler-optimizations) — 去虚拟化优化的完整图景
- [模板元编程](/topics/template-metaprogramming) — CRTP 的高级应用
- [对象模型与内存布局](/topics/memory-model) — 内存对齐与布局优化
- Itanium C++ ABI 规范 — https://itanium-cxx-abi.github.io/cxx-abi/abi.html
- LLVM HowToSetUpLLVMStyleRTTI — https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
- C++ Core Guidelines C.35, C.127 — 虚析构规则
