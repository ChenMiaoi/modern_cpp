---
title: "C++ ABI 深度解析"
topic: topics
feature: abi
status_checked_at: 2026-06-02
standard: N/A
---
# C++ ABI 深度解析

> ABI（Application Binary Interface）是 C++ 二进制兼容性的基石。你写的每一行代码——函数签名、类布局、异常传播、虚函数调度——最终都必须遵循某个 ABI 规范，否则链接失败、运行崩溃。理解 ABI 不是学术兴趣，它是跨编译器协作、动态库兼容、系统级编程的生存技能。

---

## 一、什么是 ABI

ABI 定义了**编译后的二进制代码之间的接口契约**：

```
API（源码层）                     ABI（二进制层）
┌──────────────────────┐        ┌──────────────────────────────┐
│ 函数签名             │   →    │ 名称修饰（name mangling）     │
│ 类型声明             │   →    │ 数据布局（size, align, 偏移） │
│ 参数列表             │   →    │ 调用约定（寄存器/栈传递顺序） │
│ 虚函数声明           │   →    │ vtable 布局与偏移              │
│ 异常类型             │   →    │ 异常传播与栈展开机制           │
│ 模板实例化           │   →    │ 符号导出/弱符号合并规则        │
└──────────────────────┘        └──────────────────────────────┘
```

核心区别：**API 是给编译器看的源码接口，ABI 是给链接器和操作系统看的二进制接口**。两个编译单元只要 ABI 兼容，即使源码完全不同也能链接在一起。

---

## 二、ABI vs API：关键区别

```
场景：你发布了一个动态库 libfoo.so

API 兼容（源码级）：
  · 新增函数/重载        → ✅ 现有客户端源码无需修改，重新编译即可
  · 改变函数默认参数     → ❌ 调用方语义改变，但编译可能通过（危险！）
  · 新增虚函数           → ❌ vtable 布局改变，已有客户端必须重新编译

ABI 兼容（二进制级）：
  · 末尾新增非虚函数     → ✅ 无需重新编译客户端
  · 改变类的成员变量顺序 → ❌ 偏移改变，已有客户端崩溃
  · 改变 enum 的底层类型 → ❌ sizeof 改变，布局不兼容
```

**ABI 稳定**的含义：不重新编译旧客户端，新版本的 `.so`/`.dll` 可以直接替换旧版本且运行正确。这比 API 稳定严格得多。

---

## 三、名称修饰（Name Mangling）

链接器通过符号名区分不同的函数。C++ 支持函数重载、命名空间、模板，必须将这些信息编码到符号名中。

### 3.1 Itanium C++ ABI（GCC / Clang）

```
源码：
  namespace ns {
      int foo(int x, double y);
      template<typename T> class Bar {
          void method(T, T*);
      };
      template class Bar<int>;  // 显式实例化
  }

修饰后：
  _ZN2ns3fooEid          → ns::foo(int, double)
  _ZN2ns3BarIiE6methodEiPS0_
       │   │    │    │ │  └─ T* = int*（S0 = 第0个 substitution = ns::Bar<int>）
       │   │    │    │ └──── int
       │   │    │    └────── E 参数列表结束
       │   │    └─────────── method（方法名，长度前缀编码）
       │   └──────────────── Bar<int>（模板实例化）
       └──────────────────── ns（命名空间，长度前缀编码）

编码规则：
  · N...E          → 嵌套名称段（N 开头，E 结尾）
  · 数字 + 字符串  → 长度前缀名称（2ns = "ns"，3foo = "foo"）
  · i / d / f / ...→ 内置类型缩写（i=int, d=double, f=float, v=void）
  · P              → 指针修饰（P + 基类型）
  · R              → 引用修饰
  · S_, S0_, S1_...→ substitution（重复类型的反向引用，减少符号长度）
```

**GCC 可视化工具**：

```bash
# 反修饰查看符号
$ c++filt _ZN2ns3fooEid
ns::foo(int, double)

# 从二进制中提取 C++ 符号
$ nm --demangle libfoo.so | grep 'foo'
$ readelf -sW libfoo.so | c++filt
```

### 3.2 MSVC 名称修饰

```
源码：int ns::foo(int x, double y)

MSVC 修饰：
  ?foo@ns@@YAHHN@Z
   │   │  │││ │ └─ Z 结尾
   │   │  │││ └─── N = double
   │   │  ││└───── H = int（返回类型）
   │   │  │└────── Y（__cdecl 调用约定，全局函数）
   │   │  └─────── A（public 访问级别，命名空间函数默认）
   │   └────────── ns
   └────────────── foo

MSVC 类型码：
  H = int,  N = double,  M = float,  D = char
  PA = 指针,  AA = 引用
  V = 类/结构体（按名称编码）
  ?A = 数组
```

**MSVC 可视化工具**：

```cpp
// 使用 undname.exe（随 Visual Studio 附带）
// undname ?foo@ns@@YAHHN@Z
// 输出：int __cdecl ns::foo(int, double)

// 或在代码中
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
char buf[1024];
UnDecorateSymbolName("?foo@ns@@YAHHN@Z", buf, 1024, UNDNAME_COMPLETE);
```

---

## 四、虚表（vtable）ABI 布局

虚函数通过 vtable 实现动态分派。Itanium C++ ABI 定义了精确的 vtable 内存布局：

```
单继承情况：

class Base {
    int a;
    virtual void f();
    virtual void g();
};

class Derived : public Base {
    int b;
    void f() override;   // 覆盖 Base::f
    virtual void h();    // 新增虚函数
};

Base 的 vtable：
┌─────────────────────────┐
│ offset_to_top = 0       │  ← 指向完整对象起始的偏移
│ typeinfo for Base       │  ← RTTI 指针
├─────────────────────────┤
│ &Base::f()              │  ← 槽位 0
│ &Base::g()              │  ← 槽位 1
└─────────────────────────┘

Derived 的 vtable：
┌─────────────────────────┐
│ offset_to_top = 0       │
│ typeinfo for Derived    │
├─────────────────────────┤
│ &Derived::f()           │  ← 槽位 0（覆盖了 Base::f）
│ &Base::g()              │  ← 槽位 1（继承，未覆盖）
│ &Derived::h()           │  ← 槽位 2（新增）
└─────────────────────────┘
```

**多重继承时，每个基类有独立的 vtable**：

```
class A { virtual void f(); };
class B { virtual void g(); };
class C : public A, public B { void f() override; void g() override; };

C 对象的内存布局：
┌─────────────────────┐ offset 0
│ A 子对象             │
│   vptr_A ──────────────→ C 的 A 部分 vtable
│   ...               │        ┌──────────────────────┐
├─────────────────────┤        │ offset_to_top = 0    │
│ B 子对象             │        │ typeinfo for C       │
│   vptr_B ──────────────→    ├──────────────────────┤
│   ...               │        │ &C::f()              │
└─────────────────────┘        └──────────────────────┘

                                C 的 B 部分 vtable
                                ┌──────────────────────┐
                                │ offset_to_top = -16  │ ← 回退到 C 对象起始
                                │ typeinfo for C       │
                                ├──────────────────────┤
                                │ &C::g()              │  ← thunk 实现偏移修正
                                └──────────────────────┘

B 部分的 thunk（编译器生成的跳板）：
  thunk for C::g() via B vtable:
      this -= 16;      // 修正 this 指针，从 B 子对象偏移到 C 完整对象
      jmp C::g();
```

**vptr 通常位于对象的第一个字节**（如果基类有虚函数）。对象大小 = 所有基类子对象 + vptr + 数据成员 + 末尾 padding 对齐。

---

## 五、异常处理 ABI（Itanium EH ABI）

### 5.1 异常抛出与捕获

```
抛出流程（简化）：
  throw std::runtime_error("fail");

  1. 编译器在栈上构造异常对象（可能通过 __cxa_allocate_exception）
  2. 调用 __cxa_throw(exception_obj, typeinfo, destructor)
  3. 运行时开始栈展开（unwinding）：
     a. 读取当前函数的 .eh_frame / .gcc_except_table 段
     b. 按 IP（指令指针）查找当前执行位置对应的 LSDA（Language Specific Data Area）
     c. LSDA 指定哪些 catch 能捕获当前异常类型
     d. 若匹配，跳转到 catch 块
     e. 若不匹配，调用析构函数（栈上局部对象），回退到调用者，重复 b-d
  4. 若展开到栈顶仍无匹配 → std::terminate()
```

### 5.2 .eh_frame 与 DWARF 展开信息

```
.eh_frame 段结构：

┌─────────────────────────────────────────────────────┐
│ CIE（Common Information Entry）— 每个编译单元一个   │
│  · 版本、指针编码、返回地址寄存器列                 │
│  · 初始行状态（哪些寄存器在哪里）                   │
├─────────────────────────────────────────────────────┤
│ FDE（Frame Description Entry）— 每个函数一个        │
│  · 函数起始地址、长度                               │
│  · 指令序列：如何恢复每一帧的寄存器状态             │
│  · LSDA 指针 → 异常处理表                           │
└─────────────────────────────────────────────────────┘

LSDA（Language Specific Data Area）：

┌───────────────────────────────────────────┐
│ LPStart（landing pad 基地址）              │
│ TTBase（typeinfo 表基址）                  │
│ Call Site Table                            │
│  ┌─────────────────────────────────────┐  │
│  │ [start, length, landing_pad, action]│  │  ← 每个可能抛异常的调用点
│  │ [start, length, landing_pad, action]│  │
│  └─────────────────────────────────────┘  │
│ Action Table                               │
│  ┌─────────────────────────────────────┐  │
│  │ [type_filter, next_action]          │  │  ← 串联的 catch 类型匹配链
│  └─────────────────────────────────────┘  │
└───────────────────────────────────────────┘
```

### 5.3 异常对象内存管理

```cpp
// __cxa_allocate_exception 分配的异常对象布局（Itanium ABI）：
// ┌───────────────────────────────┐
// │ __cxa_exception 头部          │  ← 运行时内部管理字段
// │  · refcount                   │
// │  · exceptionType (std::type_info*) │
// │  · exceptionDestructor        │
// │  · unexpectedHandler          │
// │  · terminateHandler           │
// │  · nextException（异常链）    │
// │  · handlerCount               │
// │  · unwindHeader (unwinder 使用) │
// ├───────────────────────────────┤
// │ 用户异常对象                   │  ← throw 表达式的类型
// └───────────────────────────────┘
//
// catch(std::exception& e) 时，e 指向用户异常对象的起始地址
// __cxa_begin_catch / __cxa_end_catch 管理引用计数
```

### 5.4 noexcept 的 ABI 影响

```cpp
// noexcept 是函数类型的一部分，影响 mangled name：
void f() noexcept;    // _Z1fv（没有 noexcept 标记——这与传统实现不同）
void g();             // _Z1gv

// 但实际上 noexcept 影响的是：
// 1. 编译器是否生成 .eh_frame FDE 条目（noexcept 函数可能不生成）
// 2. 异常传播路径（经过 noexcept 函数会直接 terminate）
// 3. 移动操作的优化（noexcept 移动构造函数允许 vector 批量移动）
```

---

## 六、RTTI ABI

```
std::type_info 对象的 ABI 布局（Itanium C++ ABI）：

┌──────────────────────────────────┐
│ vptr                              │  ← type_info 自身有虚表
│ __name (const char*)              │  ← 指向 mangled 类型名
└──────────────────────────────────┘

  对于有虚函数的类：
┌──────────────────────────────────┐
│ __type_info 的子类 vtable         │
│  · __do_catch()                   │  ← catch 匹配
│  · __do_upcast()                  │  ← 向上转型检测
│ __name                            │
│ __base_type (指向基类 type_info)  │  ← 用于 dynamic_cast
└──────────────────────────────────┘

typeid 机制：
  · 对于多态类型（有虚函数）：运行时从 vtable 中的 typeinfo 指针获取
  · 对于非多态类型：编译期确定，返回静态 type_info 对象

// 多态类型的 typeid 是运行时查询：
class Base { virtual ~Base() = default; };
class Derived : Base {};
Base* p = new Derived;
typeid(*p).name();    // "7Derived"（运行时从 vtable 中读取 RTTI）
typeid(*p) == typeid(Derived);  // true（运行时比较 type_info 指针或字符串）
```

---

## 七、参数传递调用约定

### 7.1 SysV AMD64 ABI（Linux / macOS / BSD）

```
函数参数传递规则：

整数/指针参数（rdi, rsi, rdx, rcx, r8, r9）：
  void f(int a, int b, int c, int d, int e, int f, int g);
  // a→rdi, b→rsi, c→rdx, d→rcx, e→r8, f→r9, g→栈

浮点参数（xmm0-xmm7）：
  void g(double a, double b, ..., double h, double i);
  // a→xmm0, ..., h→xmm7, i→栈

混合参数：
  void h(int a, double b, int c, double d);
  // a→rdi, b→xmm0, c→rsi, d→xmm1
  // 整数和浮点使用独立的寄存器集合，互不干扰

返回值：
  · 整数/指针 → rax（64-bit）或 rax+rdx（128-bit）
  · 浮点 → xmm0
  · 大于 16 字节的返回值 → 调用者分配空间，通过隐藏的第一个指针参数传递

结构体传递（SysV AMD64 分类规则）：
  · ≤ 16 字节且每个 8 字节段都是 INTEGER → 通过 rdi/rsi 传递
  · ≤ 16 字节且每个 8 字节段都是 SSE → 通过 xmm0/xmm1 传递
  · > 16 字节或含非整数/非 SSE 成分 → 通过引用传递（隐式指针）

示例：
  struct Small { int a; int b; };        // 8 字节 → INTEGER → rdi
  struct Mixed { int a; float b; };      // 8 字节 → INTEGER → rdi（float 归入 INTEGER）
  struct Large { int a; int b; int c; }; // 12 字节 → ≤16 → rdi（仍 INTEGER）
  struct Huge  { int a[10]; };           // 40 字节 → 通过栈/引用传递
```

### 7.2 MSVC x64 调用约定

```
MSVC x64（__fastcall，唯一可用的用户态调用约定）：

  · 前 4 个整数/指针参数 → rcx, rdx, r8, r9
  · 前 4 个浮点参数 → xmm0, xmm1, xmm2, xmm3
  · 混合时：每类各取各的（与 SysV 类似但更简单）
  · 第 5 个及以后 → 全部走栈
  · 调用者保证 32 字节的 shadow space（寄存器溢出区）

示例：
  void f(int a, int b, int c, int d, int e);
  // a→rcx, b→rdx, c→r8, d→r9, e→栈[0]
  // 调用者必须预留至少 32+8=40 字节栈空间

结构体传递（MSVC x64）：
  · ≤ 8 字节 → 通过 rcx 传递（整数）或 xmm0（浮点）
  · ≤ 16 字节 → 拆为两个 8 字节段
  · > 16 字节（但 ≤ 对齐要求） → 通过引用传递（__fastcall 特殊规则）
  · 与 SysV 的关键区别：MSVC 对含非平凡构造/析构的类型始终通过引用传递

// 通过引用传递（MSVC 特有）：
struct HasDtor {
    int x;
    ~HasDtor();  // 非平凡析构 → 即使只有 4 字节，也通过隐藏指针传递
};
```

---

## 八、结构体布局与填充规则

```cpp
struct Example {
    char  a;    // 1 字节
    // 3 字节 padding（对齐 int 到 4 字节边界）
    int   b;    // 4 字节
    char  c;    // 1 字节
    // 7 字节 padding（对齐 double 到 8 字节边界）
    double d;   // 8 字节
    char  e;    // 1 字节
    // 3 字节 padding（结构体尾部对齐到最大成员 = 8 字节）
};
// sizeof(Example) = 1+3+4+1+7+8+1+3 = 28 字节

// 内存布局图：
// 偏移  内容
// 0x00  [a]
// 0x01  [pad][pad][pad]
// 0x04  [b b b b]
// 0x08  [c]
// 0x09  [pad]×7
// 0x10  [d d d d d d d d]
// 0x18  [e]
// 0x19  [pad]×3
// 0x1C  ← sizeof = 28
```

**优化：按对齐递减排序成员可以消除 padding**：

```cpp
struct Optimized {
    double d;   // 8 字节，偏移 0x00
    int    b;   // 4 字节，偏移 0x08
    char   a;   // 1 字节，偏移 0x0C
    char   c;   // 1 字节，偏移 0x0D
    char   e;   // 1 字节，偏移 0x0E
    // 1 字节 padding（尾部对齐到 8 字节）
};
// sizeof(Optimized) = 16 字节（节省了 12 字节！）
```

**`#pragma pack` 与 `alignas` 的 ABI 影响**：

```cpp
#pragma pack(push, 1)   // 按 1 字节对齐——无 padding
struct Packed {
    char  a;  // 偏移 0
    int   b;  // 偏移 1（非对齐！）
    char  c;  // 偏移 5
};
// sizeof(Packed) = 6，但访问 b 可能跨缓存行，性能极差
#pragma pack(pop)

// alignas 提高对齐——用于 SIMD 向量对齐
struct alignas(32) AVXVec {
    float data[8];  // 32 字节对齐，适合 AVX 操作
};
```

---

## 九、ABI 稳定性与演进

### 9.1 ABI 稳定性的好处

```
ABI 稳定意味着：
  1. 旧的 .so/.dll 可以直接替换，无需重新编译依赖方
  2. 不同编译器版本编译的库可以混用（如果遵循相同 ABI）
  3. 操作系统可以独立于应用升级 C++ 运行时
  4. 分发二进制包（.deb, .rpm, NuGet）成为可能
```

### 9.2 破坏 ABI 的常见操作

```
ABI 兼容操作（可以安全发布新版本）：
  · 新增非虚成员函数
  · 新增非虚函数重载
  · 新增模板函数/类的特化
  · 改变函数体（不改变签名）
  · 改变默认参数值（需重新编译才生效，但不破坏已有二进制）

ABI 破坏操作（必须重新编译所有依赖方）：
  · 新增/删除/重排虚函数
  · 新增/删除/重排数据成员
  · 改变成员类型（即使大小相同，ABI 也可能不同）
  · 改变 enum 的底层类型或值
  · 改变 class 的访问控制顺序（改变内存布局）
  · 添加/删除虚基类
  · 改变模板参数数量或默认值
```

---

## 十、标准库的 ABI 版本化策略

### 10.1 libstdc++ 双 ABI（`__cxx11` 命名空间）

GCC 5 引入了 `std::__cxx11` 命名空间来处理 `std::string` 和 `std::list` 的 ABI 破坏性更改：

```cpp
// GCC 5 之前的 std::string（COW - Copy-On-Write）
// _ZNSsC1Ev  →  std::string::string()

// GCC 5 之后的 std::string（SSO - Small String Optimization，非 COW）
// _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev
// 即 std::__cxx11::basic_string<...>::basic_string()

// 编译时自动选择（默认 GCC 5+）：
// -D_GLIBCXX_USE_CXX11_ABI=1   → 新 ABI（默认，__cxx11 命名空间）
// -D_GLIBCXX_USE_CXX11_ABI=0   → 旧 ABI（兼容 GCC 4.x）

// 同一项目中混合两个 ABI：
void legacy_api(std::string& s);  // 编译时 ABI=0 → 旧 ABI 符号
void new_api(std::string& s);     // 编译时 ABI=1 → 新 ABI 符号
// 链接时：两个符号不同，不冲突

// 双 ABI 的 mangled name 对比：
// 旧：_ZNSsC1Ev                     → std::string::string()
// 新：_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev
//           ↑ 关键区别：__cxx11 命名空间段
```

**诊断工具**：

```bash
# 检查 .so 使用的是哪个 ABI
$ nm -DC libfoo.so | grep 'std::__cxx11'
# 有输出 → 使用新 ABI

$ nm -DC libfoo.so | grep 'std::basic_string' | grep -v '__cxx11'
# 有输出 → 使用旧 ABI

# 检查 ABI 兼容性
$ abi-compliance-checker -lib libfoo -old old.xml -new new.xml
```

### 10.2 libc++ 内联命名空间版本化

```cpp
// libc++ 使用内联命名空间（inline namespace）实现 ABI 版本化
// 每个版本有独立的命名空间，符号名自动包含版本标记

namespace std {
    inline namespace __1 {    // libc++ 的默认 ABI 版本
        template<class CharT, class Traits, class Allocator>
        class basic_string;
    }
}

// mangled name 包含 __1：
// _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1Ev

// 当 ABI 需要破坏性更改时，引入新版本：
// _LIBCPP_ABI_VERSION 1 → inline namespace __1（当前默认）
// _LIBCPP_ABI_VERSION 2 → inline namespace __2（实验性）

// 版本间符号不冲突，可以同时链接两个版本的类型：
// -D_LIBCPP_ABI_VERSION=1  → std::__1::string
// -D_LIBCPP_ABI_VERSION=2  → std::__2::string

// libc++ 特定 ABI 选项（通过宏控制）：
// _LIBCPP_ABI_ALTERNATE_STRING_LAYOUT      → 改变 string 的内部布局
// _LIBCPP_ABI_UNSTABLE                      → 启用所有实验性 ABI 破坏
// _LIBCPP_ABI_NO_PAIR_COMPARISON            → 移除 std::pair 的过时比较运算符
```

### 10.3 MSVC `_ITERATOR_DEBUG_LEVEL`

MSVC 使用 `_ITERATOR_DEBUG_LEVEL` 控制迭代器检查的 ABI 级别：

```cpp
// _ITERATOR_DEBUG_LEVEL=0 → 禁用所有迭代器检查（发布模式默认）
// _ITERATOR_DEBUG_LEVEL=1 → 基本检查（调试模式默认）
// _ITERATOR_DEBUG_LEVEL=2 → 完整检查（边界检查、悬挂迭代器检测）

// 不同级别生成不同的符号，不能混用：
// IDL=0: std::vector<int>::iterator 是原始指针
// IDL=2: std::vector<int>::iterator 是包装类（含容器指针、版本号）

// 编译时设置：
// cl /D_ITERATOR_DEBUG_LEVEL=0 mylib.cpp   // 发布库
// cl /D_ITERATOR_DEBUG_LEVEL=2 myapp.cpp   // 调试应用
// 链接 mylib.obj + myapp.obj → ❌ 链接错误：符号冲突

// IDL=2 的 iterator 布局：
struct _Vector_iterator_debug {
    int* _Ptr;               // 当前元素指针
    const _Container_base* _Mycont;  // 指向所属容器
    // 每次迭代器操作都验证：
    //   _Mycont != nullptr（未脱离容器）
    //   _Ptr 在容器范围内
    //   容器版本号未改变（检测迭代中修改容器）
};
```

---

## 十一、ABI 破坏检测工具

```bash
# 1. abi-compliance-checker — 最流行的 ABI 兼容性检查工具
#    比较两个版本的库，生成详细的差异报告
$ abi-compliance-checker -lib libfoo \
    -old old_headers/include -new new_headers/include \
    -old-abi old.dump -new-abi new.dump

# 输出：HTML 报告，列出所有 ABI 破坏：
#   - 移除的符号
#   - 改变的参数类型
#   - 改变的数据成员布局
#   - 改变的 vtable 布局

# 2. abidiff — libabigail 的核心工具
$ abidiff libfoo_v1.so libfoo_v2.so
# 输出：人类可读的 ABI 差异文本

# 3. libabigail 的 ABI 抑制机制
# 创建 suppress.abignore 文件：
[suppress_type]
  name_regexp = .*_detail.*
  # 忽略所有 _detail 命名空间中的类型变化

# 4. nm / readelf — 手动比较符号
$ nm -DC libfoo_v1.so | sort > v1.symbols
$ nm -DC libfoo_v2.so | sort > v2.symbols
$ diff v1.symbols v2.symbols
# 移除的符号 = ABI 破坏

# 5. ldd / objdump — 检查动态依赖
$ objdump -p libfoo.so | grep NEEDED
# 检查依赖的库版本是否兼容

# 6. MSVC: dumpbin /EXPORTS
$ dumpbin /EXPORTS foo_v1.dll > v1_exports.txt
$ dumpbin /EXPORTS foo_v2.dll > v2_exports.txt
$ fc v1_exports.txt v2_exports.txt
```

---

## 十二、符号可见性（Symbol Visibility）

### 12.1 为什么需要控制可见性

```
默认行为（-fvisibility=default）：
  · 所有符号都被导出到 .so 的动态符号表
  · 动态链接器可以解析所有导出符号
  · 问题：符号表臃肿、链接慢、符号冲突风险高

目标：
  · 仅导出公共 API 符号（最小可见性原则）
  · 内部实现符号对外不可见 → 减少符号表大小、加速链接、避免冲突
  · 允许编译器更多优化（隐藏符号可以被去虚拟化、内联）
```

### 12.2 GCC/Clang 可见性控制

```cpp
// 全局默认可见性（编译选项）
// -fvisibility=hidden   → 默认隐藏所有符号
// -fvisibility=default  → 默认导出所有符号（传统行为）

// 逐符号控制：
__attribute__((visibility("default"))) void public_api();   // 导出
__attribute__((visibility("hidden")))  void internal_func(); // 隐藏

// 类级别控制：
class __attribute__((visibility("default"))) Widget {
    // 所有成员函数都被导出
    void method();  // 自动导出
};

// DLLExport/Import 风格封装：
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef BUILDING_DLL
    #define DLL_PUBLIC __declspec(dllexport)
  #else
    #define DLL_PUBLIC __declspec(dllimport)
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__((visibility("default")))
    #define DLL_LOCAL  __attribute__((visibility("hidden")))
  #else
    #define DLL_PUBLIC
    #define DLL_LOCAL
  #endif
#endif

// 使用：
DLL_PUBLIC  int exported_function();    // 导出
DLL_LOCAL   void internal_helper();     // 隐藏

// 模板的可见性：
// 模板实例化使用 COMDAT folding，visibility 属性控制动态链接行为
template class DLL_PUBLIC std::vector<int>;  // 显式实例化并导出
```

### 12.3 CMake 实践

```cmake
# CMake 中设置符号可见性（推荐方式）
set(CMAKE_CXX_VISIBILITY_PRESET hidden)      # 默认隐藏
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)      # 内联函数也隐藏

# 生成 compile_commands.json 用于调试可见性问题
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows 下使用生成表达式控制导出/导入
target_compile_definitions(mylib PRIVATE
    $<$<BUILD_INTERFACE:BUILDING_DLL>
)

# 使用 export header 自动生成宏
include(GenerateExportHeader)
generate_export_header(mylib
    EXPORT_FILE_NAME export/mylib_export.h
    # 生成 MYLIB_EXPORT 和 MYLIB_NO_EXPORT 宏
)
```

### 12.4 可见性对性能的影响

```
隐藏符号的优化收益：

  1. 去虚拟化（devirtualization）：
     // 当编译器知道所有可能的派生类都在同一个 .so 中
     // （因为派生类的符号被隐藏，无法从外部链接）
     // 编译器可以将虚函数调用替换为直接调用

  2. 内联（inlining）：
     // 隐藏符号的函数体在编译单元内可见
     // 链接时优化（LTO）可以内联它们

  3. 减少 PLT/GOT 开销：
     // 默认可见性 → 通过 PLT 间接调用（4-5 条额外指令）
     // 隐藏可见性 → 直接调用（0 条额外指令）
     // 在热循环中这可以产生可测量的性能差异

  符号表大小对比（典型情况）：
  ┌──────────────────────────────────────────┐
  │ 100 个公共 API + 5000 个内部符号         │
  │                                          │
  │ -fvisibility=default: 5100 个导出符号    │
  │ -fvisibility=hidden + 100 个 default:    │
  │                       100 个导出符号     │
  │                                          │
  │ 动态链接时间：~50x 差异                  │
  └──────────────────────────────────────────┘
```

---

## 十三、跨平台 ABI 兼容实践

```
编写跨编译器、跨平台的 ABI 稳定库的核心原则：

  1. 使用 C 语言接口（extern "C"）作为公共 API 边界
     extern "C" void* widget_create(int width, int height);
     extern "C" void  widget_destroy(void* handle);
     // C ABI 跨编译器完全兼容，无 mangle、无 vtable、无异常

  2. 不在公共头文件中暴露 C++ 类
     // .h  — 只暴露 C 接口和 opaque 指针
     // .cpp — 内部实现使用完整的 C++ 特性

  3. PIMPL（Pointer to Implementation）隐藏私有成员
     // widget.h
     class Widget {
     public:
         Widget(int w, int h);
         ~Widget();
         void draw();
     private:
         struct Impl;         // 前置声明
         std::unique_ptr<Impl> impl_;  // 大小固定（一个指针）
     };
     // 添加私有成员不改变 Widget 的大小和布局 → ABI 稳定

  4. 使用 feature-test 宏避免头文件条件编译差异
     #if __has_include(<format>)
     #include <format>
     // 新接口
     #endif

  5. 永远不在公共 ABI 中使用 STL 类型
     // ❌ std::string, std::vector 在公共 API 中——不同编译器/版本不兼容
     // ✅ const char* + size_t，或自定义 ABI 稳定的容器类型
```

---

## 延伸阅读

- [Itanium C++ ABI 规范](https://itanium-cxx-abi.github.io/cxx-abi/) — GCC / Clang 遵循的权威 ABI 文档
- [SysV AMD64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI) — x86-64 平台调用约定与数据布局规范
- [libstdc++ Dual ABI 文档](https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html) — GCC 双 ABI 迁移指南
- [libc++ ABI 版本化](https://libcxx.llvm.org/DesignDocs/ABIVersioning.html) — LLVM libc++ 的内联命名空间策略
- [abi-compliance-checker](https://lvc.github.io/abi-compliance-checker/) — ABI 兼容性自动检查工具
- [C++ 工具链与生态](/topics/toolchain) — 编译器、构建系统、sanitizers
- [编译器优化全景](/topics/compiler-optimizations) — LTO、去虚拟化、内联等影响 ABI 的优化
