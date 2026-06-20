---
title: "虚函数表（vtable）实现机制"
topic: internals
feature: vtable
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h"
---

# 虚函数表（vtable）实现机制

> C++ 的多态并非魔法——它是一套由编译器自动生成的函数指针表（vtable）和类型元数据（type_info）构成的精确定向机制。本文基于 Itanium C++ ABI 规范，分析 GCC 和 LLVM 如何实现这套机制。

---

## 一、vtable 的基本概念

### 1.1 什么是 vtable

虚函数表（Virtual Table，vtable）是 C++ 实现运行时多态的核心数据结构。每个包含虚函数的类都有一个静态的 vtable，每个包含虚函数的对象都持有一个指向 vtable 的指针（vptr）。

```
vtable 的作用：
┌─────────────────────────────────────────────────────────┐
│  编译期：编译器为每个含虚函数的类生成一个 vtable          │
│  运行时：通过 vptr 查找 vtable，间接调用正确的函数        │
│  布局：vptr 通常位于对象首地址（偏移 0）                  │
└─────────────────────────────────────────────────────────┘
```

### 1.2 vptr 的位置

在 Itanium C++ ABI（GCC/Clang 在类 Unix 平台上的事实标准）中，vptr 位于对象起始地址：

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
```

---

## 二、GCC (libstdc++) 的虚函数实现

### 2.1 vtable 的生成

GCC 在编译期为每个含虚函数的类生成 vtable。vtable 是一个静态数组，包含：

1. **虚函数指针**：按声明顺序排列
2. **RTTI 指针**：指向 `type_info` 对象
3. **offset_to_top**：到完整对象起始地址的偏移

```
单继承情况下的 vtable 布局：

class Base {
    virtual void f1();
    virtual void f2();
    virtual ~Base();
};

class Derived : public Base {
    void f1() override;
    virtual void f4();
};

Base 的 vtable：
┌──────────────────────────────┬──────────────────────┐
│ 索引 │ 内容                  │ 说明                  │
├──────┼───────────────────────┼──────────────────────┤
│  -3  │ offset_to_top        │ 0（到主基类的偏移）   │
│  -2  │ RTTI pointer         │ typeinfo_for_Base     │
│  -1  │ ...                  │ （ABI 预留槽位）       │
│   0  │ Base::f1()           │ 第一个虚函数          │
│   1  │ Base::f2()           │                       │
│   2  │ Base::~Base()        │ 虚析构函数            │
└──────┴──────────────────────┴──────────────────────┘

Derived 的 vtable：
┌──────┬───────────────────────┬──────────────────────┐
│ 索引 │ 内容                  │ 说明                  │
├──────┼───────────────────────┼──────────────────────┤
│  -3  │ offset_to_top        │ 0（单继承，无偏移）   │
│  -2  │ RTTI pointer         │ typeinfo_for_Derived  │
│  -1  │ ...                  │                       │
│   0  │ Derived::f1()        │ 覆盖了 Base::f1       │
│   1  │ Base::f2()           │ 继承                  │
│   2  │ Derived::~Derived()  │ 覆盖析构              │
│   3  │ Derived::f4()        │ 追加在末尾            │
└──────┴───────────────────────┴──────────────────────┘
```

### 2.2 vptr 的初始化

GCC 在每个构造函数体前插入 vptr 赋值代码：

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

### 2.3 GCC 的 vtable 实现特点

GCC 使用 **单一 vtable 模式**：
- 每个类只有一个 vtable（包含所有虚函数）
- vptr 在构造函数中被多次写入（每个基类构造函数写一次）
- 最终在最派生类构造函数中设置为正确的 vtable

```
构造 D 对象时的 vptr 变化（D 继承自 B，B 继承自 A）：

D::D() {
    A::A();  // vptr → vtable_for_A
    B::B();  // vptr → vtable_for_B
    // D 自身构造：vptr → vtable_for_D
}
```

---

## 三、LLVM (libc++) 的虚函数实现

### 3.1 vtable 的生成

LLVM 的 libc++ 遵循相同的 Itanium ABI 规范，但有一些实现细节上的差异：

```
LLVM 的 vtable 布局与 GCC 完全相同：
- 负偏移：offset_to_top (-3), RTTI pointer (-2)
- 正偏移：虚函数指针（按声明顺序）
- 虚析构函数：两个槽位（deleting destructor + complete destructor）
```

### 3.2 LLVM 的 vtable 实现特点

LLVM 使用 **相同的 vtable 结构**，但有以下特点：

1. **相同的 vptr 初始化策略**：构造函数中多次写入
2. **相同的 thunk 机制**：多重继承时调整 this 指针
3. **相同的 VTT 机制**：虚继承时动态调整 vtable 偏移

---

## 四、多重继承下的 vtable 布局

### 4.1 多个 vptr

多重继承时，每个基类子对象都有自己的 vptr：

```
class A { virtual void f(); int a; };
class B { virtual void g(); int b; };
class C : public A, public B {
    void f() override;
    void g() override;
    int c;
};

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
```

### 4.2 Thunk：this 指针调整

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

## 五、虚继承与 VTT

### 5.1 虚继承的菱形问题

```
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

### 5.2 VTT（Virtual Table Table）

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
```

---

## 六、虚析构函数的两个槽位

Itanium ABI 为虚析构函数保留两个槽位：

```
deleting destructor（删除型）：
    调用完整析构后，释放对象内存（operator delete）
    用于 delete p; 场景

complete destructor（完整型）：
    调用析构函数体，但不释放内存
    用于栈上对象析构、placement delete 等场景

base destructor（基类析构）：
    仅析构基类和成员，不涉及派生类部分
    由 derived destructor 调用，不出现在 vtable 中
```

---

## 七、GCC vs LLVM 的 vtable 实现对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ vtable 结构            │ Itanium ABI          │ Itanium ABI          │
│ vptr 位置              │ 对象起始地址         │ 对象起始地址         │
│ 虚析构槽位数           │ 2（deleting+complete）│ 2（deleting+complete）│
│ thunk 机制             │ 完全相同             │ 完全相同             │
│ VTT 支持               │ 完全相同             │ 完全相同             │
│ 构造时 vptr 写入次数   │ 每个基类一次         │ 每个基类一次         │
│ -fno-rtti 支持         │ 完全相同             │ 完全相同             │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

**关键结论**：GCC 和 LLVM 的 vtable 实现几乎完全相同，因为它们都遵循 Itanium C++ ABI 规范。差异主要在于编译器优化策略（如去虚拟化）和 RTTI 的具体实现细节。

---

## 八、vtable 的开销模型

```
空间开销：
  ┌───────────────────────────────────────────────────────┐
  │ 每个多态类：                                          │
  │   · 1 个 vtable（大小 = 虚函数数量 × 8 字节 + 辅助）  │
  │   · 1 个 type_info 对象（约 16-32 字节）              │
  │   · 每个对象 1 个 vptr（8 字节）                      │
  │                                                       │
  │ 100 个多态类 × 平均 5 个虚函数 ≈ 100 KB               │
  └───────────────────────────────────────────────────────┘

时间开销：
  ┌───────────────────────────────────────────────────────┐
  │ 虚函数调用：                                          │
  │   · 1 次 vptr 读取                                    │
  │   · 1 次 vtable 条目读取                              │
  │   · 1 次间接跳转                                      │
  │   · 总计 ~几纳秒（比直接调用慢 2-3 倍）               │
  │                                                       │
  │ 真正的性能问题：                                       │
  │   · 虚函数阻碍内联 → 放大调用链的累积开销             │
  │   · 虚函数的内存布局不利于 cache                      │
  └───────────────────────────────────────────────────────┘
```

---

## 九、去虚拟化（Devirtualization）

编译器在可能的情况下将虚调用替换为直接调用，消除间接跳转开销并允许内联：

```cpp
// 场景 1：已知具体类型
Derived d;
Base& b = d;
b.speak();  // 编译器知道 b 绑定到 Derived → 直接调用 Derived::speak()

// 场景 2：final 类
class Final final : public Base {
    void speak() override final;  // 无子类可覆盖
};

// 场景 3：LTO（链接时优化）— 跨翻译单元可见全部层次结构
```

### LLVM 的去虚拟化实现

```
LLVM 的去虚拟化 passes：

  1. -fstrict-vtable-pointers（标注 vptr 不变性的假设）
     在对象构造完成后到析构开始前，vptr 不变。
     LLVM 插入 llvm.assume 来标注这一点。

  2. GlobalDCE pass（IPO pass）
     · 收集所有虚函数覆盖关系
     · 若某个虚函数只有一个可能的实现 → 替换为直接调用
     · 结合 LTO 效果最佳

  3. 标注（attributes）：
     · __attribute__((annotate("vtable_visibility", "all")))
       告诉编译器已知全部子类
```

---

## 十、最佳实践

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

- [对象模型与内存布局](/internals/runtime/object-model) — vptr 在内存中的位置
- [RTTI 与 dynamic_cast](/internals/runtime/rtti) — type_info 的实现
- [去虚拟化实现](/internals/compiler/devirtualization) — 编译器如何优化虚函数调用
- [Itanium C++ ABI 规范](https://itanium-cxx-abi.github.io/cxx-abi/abi.html) — GCC/Clang 遵循的权威 ABI 文档
- [模板元编程](/topics/template-metaprogramming) — CRTP 的高级应用
