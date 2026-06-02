---
title: "去虚拟化"
topic: topics
feature: compiler-opt-devirtualization
standard: C++
status_checked_at: 2026-06-02
---

# 去虚拟化（Devirtualization）

> 虚函数调用是 C++ 多态的代价：一次间接跳转（vtable 查找），加上无法内联的副作用。去虚拟化将虚调用替换为直接调用，消除间接跳转开销并解锁内联。这是 C++ 性能优化中"零开销抽象"的关键一环。

---

## 虚调用的开销

```
虚调用 vs 直接调用：

  直接调用：                    虚调用：
  ┌──────────────────┐         ┌──────────────────────────────┐
  │ call _ZN5Base7 │         │ load vptr from obj           │
  │   computeEi     │         │ load fn_ptr from vtable[slot]│
  │                  │         │ call fn_ptr                  │
  │ 开销：          │         │                              │
  │ · 1 条 call 指令│         │ 开销：                       │
  │ · 目标固定      │         │ · 2 条 load（内存访问）      │
  │ · 分支预测容易  │         │ · 1 条 call 指令             │
  └──────────────────┘         │ · 间接跳转（分支预测难）    │
                               │ · 阻止内联                   │
                               └──────────────────────────────┘

  实测差异：
  · 直接调用：~1-2 cycles（分支预测命中时）
  · 虚调用：~5-15 cycles（含 vtable load + 间接跳转 + I-cache miss）
  · 虚调用无法内联 → 错失常量传播、死代码消除等优化
```

---

## 编译器自动去虚拟化

### 类型推断去虚拟化

当编译器能静态推断对象的动态类型时，可以直接替换虚调用：

```cpp
class Base {
public:
    virtual int compute(int x) { return x; }
};

class Derived : public Base {
    int compute(int x) override { return x * 2; }
};

void direct_call() {
    Derived d;
    d.compute(42);  // 编译器知道 d 的类型一定是 Derived
                     // → 直接调用 Derived::compute，无需 vtable
}
```

```bash
# 查看去虚拟化效果
clang++ -O2 -S -masm=intel test.cpp -o test.s
# 搜索汇编：如果是 call _ZN7Derived7computeEi → 去虚拟化成功
#           如果是 call rax（间接跳转）→ 未去虚拟化

# GCC 查看去虚拟化决策
g++ -O2 -fdump-tree-fre test.cpp
# FRE 是 GCC 中进行早期去虚拟化的 pass
```

### final 类/方法的去虚拟化

```cpp
class Widget final {  // final 类：不可能有子类
public:
    virtual int process(int x) { return x + 1; }
};

void call(Widget& w) {
    w.process(42);  // Widget 是 final → 编译器知道动态类型
                    // → 直接调用 Widget::process
}

class Engine {
public:
    virtual int run(int x) final { return x; }  // final 方法
    // 即使有子类，子类也不能 override run()
    // → 对 Engine 类型的引用，run() 可以去虚拟化
};
```

### final 关键字的直接效果

```cpp
class Base {
public:
    virtual int f() { return 0; }
};

class Derived final : public Base {
    int f() override { return 1; }
};

// 当 Derived 是 final 时，编译器知道：
// Derived* 指向的对象类型一定是 Derived（不能有子类）
// → 可以安全地将虚调用替换为直接调用
```

---

## 推测性去虚拟化（Speculative Devirtualization）

当编译器通过 PGO 数据得知某个虚调用**几乎总是**同一个目标时，会插入类型检查 + 直接调用 + 调用者回退到间接调用：

```
推测性去虚拟化前：
  call obj->process(x)          ; 每次都是间接调用

推测性去虚拟化后：
  if (typeid(obj) == typeid(Derived)) {    ; 类型守卫
      // 热路径：直接调用（可内联）
      return Derived::process(obj, x);      ; 直接调用
  } else {
      // 冷路径：保留间接调用
      return obj->process(x);               ; 间接调用
  }

  用 C++ 源码近似：
  if (auto* d = dynamic_cast<Derived*>(&obj)) {
      return d->process(x);   // 95% 的时间走这里
  } else {
      return obj.process(x);  // 5% 的时间走这里
  }
```

```bash
# GCC 推测性去虚拟化
g++ -O2 -fdevirtualize-speculatively test.cpp
# 默认在 -O2 及以上启用

# LLVM 推测性去虚拟化（需要 PGO 数据）
clang++ -O2 -fprofile-use=default.profdata test.cpp
# 编译器根据 profile 数据决定是否推测去虚拟化
```

---

## 全程序去虚拟化（Whole-Program Devirtualization）

LTO 使编译器能看到整个程序的所有类型定义和调用点：

```
无 LTO（单个翻译单元视角）：
  a.cpp: void process(Base* obj) { obj->compute(); }
  编译器不知道 Base 有哪些子类 → 无法去虚拟化

有 LTO（全程序视角）：
  链接器看到：
    Base 被 Derived1, Derived2 继承
    process() 只被传入 Derived1 类型的对象
  → 可以安全地将虚调用替换为 Derived1::compute()
```

```bash
# LLVM 全程序去虚拟化
clang++ -flto=thin -O2 -fwhole-program-vtable a.cpp b.cpp -o app

# 查看全程序去虚拟化的决策
clang++ -flto=thin -O2 -fwhole-program-vtable \
        -Rpass=wholeprogramdevirt a.cpp -c

# GCC 全程序优化
g++ -flto -O2 -fwhole-program a.cpp b.cpp -o app
# GCC 在 LTO 时自动进行跨模块去虚拟化
```

### 可见性对去虚拟化的影响

```bash
# 如果动态库导出了类型，编译器不能假设没有外部子类
# 解决方案：使用 visibility 属性限制导出

# 编译时指定可见性
clang++ -fvisibility=hidden -flto=thin -O2 a.cpp

# 或者在源码中标记
class __attribute__((visibility("hidden"))) Internal final {
    virtual int f() { return 0; }
};
```

---

## CRTP：编译期多态替代

CRTP（Curiously Recurring Template Pattern）在编译期解析多态，完全消除虚函数开销：

```cpp
// CRTP 基类
template <typename Derived>
class Shape {
public:
    void draw() const {
        // 编译期调用 Derived 的实现
        static_cast<const Derived*>(this)->draw_impl();
    }
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

// 派生类
class Circle : public Shape<Circle> {
    double radius_;
public:
    explicit Circle(double r) : radius_(r) {}
    void draw_impl() const { /* 画圆 */ }
    double area_impl() const { return 3.14159265 * radius_ * radius_; }
};

class Rect : public Shape<Rect> {
    double w_, h_;
public:
    Rect(double w, double h) : w_(w), h_(h) {}
    void draw_impl() const { /* 画矩形 */ }
    double area_impl() const { return w_ * h_; }
};

// 使用
template <typename T>
void render(const Shape<T>& shape) {
    shape.draw();       // 编译期解析 → 直接调用 → 可内联
    double a = shape.area();  // 内联后 → 常量折叠
}
```

```
虚函数 vs CRTP：

  ┌────────────────────────────────────┬──────────────────────────────┐
  │ 虚函数                             │ CRTP                         │
  │                                    │                              │
  │ 运行时多态                         │ 编译期多态                   │
  │ 间接调用（vtable 查找）           │ 直接调用（模板实例化）       │
  │ 可以异构容器                       │ 不能异构容器（类型不同）    │
  │ 一个 vtable slot（8 bytes/方法）  │ 零额外内存开销              │
  │ 可以动态添加子类                   │ 必须编译期已知所有类型      │
  │ 不可内联                           │ 完全可内联                   │
  └────────────────────────────────────┴──────────────────────────────┘
```

---

## [[likely]] / [[unlikely]] 与去虚拟化

分支提示帮助编译器在推测性去虚拟化中做出更好的布局决策：

```cpp
// C++20 分支提示
if (auto* d = dynamic_cast<FastPath*>(obj)) [[likely]] {
    d->fast_compute();   // 放在热路径 → 更好的 I-cache 局部性
} else [[unlikely]] {
    obj->slow_compute(); // 放在冷路径 → 不影响热路径性能
}
```

```bash
# GCC/Clang 都支持 [[likely]]/[[unlikely]]
# 编译器将 unlikely 分支放到 .text.unlikely 段
# 热路径保持紧凑 → I-cache 友好

# 查看代码布局
clang++ -O2 -S test.cpp -o test.s
# 检查 .text.hot 和 .text.unlikely 段的分布
```

---

## [[gnu::flatten]] 与去虚拟化

`flatten` 属性递归内联整个调用树，是去虚拟化的"大锤"：

```cpp
__attribute__((flatten))  // 内联此函数中所有可内联的调用
void hot_loop(Engine* engines, int n) {
    for (int i = 0; i < n; ++i)
        engines[i].step();  // 如果 step() 能被去虚拟化，递归内联其体
}
```

---

## 去虚拟化的局限

```
┌─────────────────────────────────────────────────────────┐
│ 去虚拟化不适用的场景                                     │
│                                                         │
│  1. 异构容器：                                          │
│     std::vector<std::unique_ptr<Base>> v;               │
│     for (auto& p : v) p->process();                     │
│     // 元素类型不一致 → 编译器无法去虚拟化              │
│     // 只有 PGO + 推测性去虚拟化可能部分解决            │
│                                                         │
│  2. 插件架构：                                          │
│     Base* obj = load_plugin(name);                      │
│     obj->execute();                                     │
│     // 动态加载 → 运行时才知道类型                      │
│                                                         │
│  3. 跨 DSO 调用：                                       │
│     // 共享库中的虚函数调用 → 编译器无法跨越 DSO 边界  │
│                                                         │
│  4. 深层继承 + 虚继承：                                 │
│     // 虚基类偏移在 vtable 中 → 更复杂的去虚拟化逻辑   │
└─────────────────────────────────────────────────────────┘
```

---

## 实战：去虚拟化效果测量

```bash
# 第 1 步：查看虚调用数量
clang++ -O2 -S test.cpp -o test.s
grep -c 'call.*rax\|call.*rdx' test.s   # 间接调用数量（近似）

# 第 2 步：禁用去虚拟化对比
clang++ -O2 -fno-devirtualize test.cpp -c
# 性能回退 = 虚调用的开销

# 第 3 步：查看编译器去虚拟化 remark
clang++ -O2 -Rpass=inline test.cpp -c
# 搜索 "virtual" 相关的内联 remark

# 第 4 步：用 perf 分析间接跳转
perf stat -e branch-misses,branches ./test_app
# 高 branch-miss 率可能表明虚调用过多
```

---

## 延伸阅读

- [内联](/topics/compiler-optimizations/inlining) — 去虚拟化后解锁内联
- [LTO](/topics/compiler-optimizations/lto) — 全程序去虚拟化的基础设施
- [PGO](/topics/compiler-optimizations/pgo) — Profile 引导推测性去虚拟化
- [C++ 编译器优化全景](/topics/compiler-optimizations) — 整体优化管线
- [模板元编程](/topics/template-metaprogramming) — CRTP 与模板技术
- [设计模式](/topics/design-patterns) — 多态在设计模式中的应用
- [值类别深度解析](/topics/value-categories-deep-dive) — 移动语义与虚函数的交互
