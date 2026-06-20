---
title: "名称修饰（Name Mangling）实现分析"
topic: internals
feature: name-mangling
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "N/A"
source_llvm: "N/A"
---

# 名称修饰（Name Mangling）实现分析

> 名称修饰是将 C++ 标识符编码为链接器可识别的符号名。本文分析 GCC 和 LLVM 的名称修饰实现。

---

## 一、核心概念

### 1.1 什么是名称修饰

名称修饰将 C++ 标识符编码为唯一的符号名：

```cpp
// C++ 代码
namespace ns {
    int foo(int x, double y);
}

// 修饰后的符号名（Itanium ABI）
_ZN2ns3fooEid
```

---

## 二、Itanium ABI 编码规则

### 2.1 编码格式

```
Itanium ABI 编码规则：

前缀：
  _Z：全局函数
  _ZN：嵌套名称

类型编码：
  i：int
  d：double
  f：float
  c：char
  v：void
  P：指针
  R：引用

名称编码：
  数字 + 字符串：长度前缀
  2ns：命名空间 "ns"
  3foo：函数名 "foo"
```

### 2.2 示例

```
编码示例：

ns::foo(int, double)
  _ZN2ns3fooEid
    │   │   │ │ └─ d = double
    │   │   │ └─── i = int
    │   │   └───── foo
    │   └───────── ns
    └───────────── N...E = 嵌套名称

std::vector<int>::push_back(int)
  _ZNSt6vectorIiSaIiEE9push_backEOi
```

---

## 三、MSVC 编码规则

### 3.1 编码格式

```
MSVC 编码规则：

前缀：
  ?：函数名
  @@：类名结束

类型码：
  H：int
  N：double
  M：float
  D：char
  PA：指针
  AA：引用
```

---

## 四、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC                  │ Clang/LLVM           │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ ABI 标准               │ Itanium ABI          │ Itanium ABI          │
│ 编码格式               │ _ZN...               │ _ZN...               │
│ 模板实例化             │ 支持                 │ 支持                 │
│ lambda                 │ 支持                 │ 支持                 │
│ c++filt                │ 反修饰工具           │ 反修饰工具           │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 五、最佳实践

```
名称修饰使用指南：

1. 使用 c++filt 反修饰：
   c++filt _ZN2ns3fooEid

2. 使用 nm 查看符号：
   nm --demangle libfoo.so

3. 使用 extern "C"：
   · 避免名称修饰
   · C 接口

4. 注意 ABI 兼容性：
   · 不同编译器可能不同
   · 使用相同的 ABI
```

---

## 延伸阅读

- [C++ ABI 深度解析](/topics/abi) — ABI 的详细解释
- [虚函数表实现](/internals/runtime/vtable) — vtable 的实现
- [GCC 编译器内部](/internals/compiler/gcc-internals) — GCC 的实现
