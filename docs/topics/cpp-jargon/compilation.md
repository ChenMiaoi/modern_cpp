# 编译与链接术语

## Translation Unit（翻译单元）

一个 `.cpp` 文件经过预处理后的内容（包括所有 `#include` 展开）。每个翻译单元独立编译为 `.o`/`.obj` 文件，然后链接器将它们合并。

## ODR（One Definition Rule，单一定义规则）

整个程序中，每个实体（函数、类、变量）只能有一个定义。违反 ODR 是 **UB**——编译器可能不报错，但程序行为不可预测。

```cpp
// a.cpp
int foo() { return 1; }

// b.cpp
int foo() { return 2; }  // ODR violation! 链接时可能不报错

// 正确做法：inline 函数必须在头文件中定义，且所有定义相同
```

## Linkage（链接性）

| 链接性 | 含义 | 示例 |
|--------|------|------|
| external | 整个程序可见 | 非 static 的全局函数/变量 |
| internal | 当前翻译单元可见 | `static` 全局函数/变量、匿名命名空间 |
| no linkage | 局部作用域 | 局部变量 |

## Static Initialization Order Fiasco

不同翻译单元中的全局变量的初始化顺序是未定义的：

```cpp
// a.cpp
std::string global_a = "hello";

// b.cpp
extern std::string global_a;
std::string global_b = global_a;  // 危险！global_a 可能尚未初始化
```

解决方案：使用函数内的 `static` 变量（Meyers Singleton）——保证在首次调用时初始化。

## ABI（Application Binary Interface）

函数在二进制层面的约定——参数如何传递、返回值如何获取、名字如何修饰。不同编译器/版本的 ABI 可能不兼容。

libstdc++ 的 `_GLIBCXX_USE_CXX11_ABI` 宏就是 ABI 版本控制的例子——通过 `__cxx11` 内联命名空间和 `abi_tag` 属性区分新旧 ABI 的符号。

## Name Mangling（名称修饰）

编译器将 C++ 函数名编码为唯一的符号名：

```cpp
namespace MyLib {
  class Widget {
    void process(int, double);
  };
}
// GCC 符号名: _ZN6MyLib6Widget7processEid
//               ─┬─  ─┬─  ─┬─  ─┬─ ─┬
//             namespace class method 参数类型
```

不同编译器的 mangling 规则不同——这是 ABI 不兼容的主要原因之一。

## PImpl（Pointer to Implementation）

隐藏实现细节、减少编译依赖的经典惯用法：

```cpp
// widget.h
class Widget {
  struct Impl;              // 前向声明
  std::unique_ptr<Impl> pImpl_;  // 指向实现
public:
  Widget();
  ~Widget();
  void do_something();
};

// widget.cpp
struct Widget::Impl {
  // 所有私有成员都在这里
  std::vector<Data> cache_;
  DatabaseConnection db_;
};

Widget::Widget() : pImpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;  // 必须在 .cpp 中，因为 Impl 是不完整类型
```

**优势**：修改 `Impl` 不需要重新编译使用 `Widget.h` 的代码。

## Forward Declaration（前向声明）

告诉编译器"这个类型存在"，不需要完整的定义：

```cpp
class Widget;  // 前向声明——只能用于指针/引用

void process(Widget* w);  // OK：只需要指针
void process(Widget w);   // 错误：需要完整定义
```

## Include Guard vs #pragma once

```cpp
// 传统 include guard
#ifndef MY_HEADER_H
#define MY_HEADER_H
// ... 头文件内容 ...
#endif

// 编译器扩展（非标准但所有主流编译器支持）
#pragma once
```

## PCH（Precompiled Header）

预编译头文件——将不常变化的头文件（如 `<iostream>`、`<vector>`）预先编译为二进制格式，加速后续编译。

## LTO（Link-Time Optimization）

链接时优化——编译器在链接阶段进行跨翻译单元的优化（内联、死代码消除等）。Gold linker 的 ThinLTO 和 GCC 的 `-flto` 都是实现。

## Translation Phases（翻译阶段）

C++ 编译分为 9 个阶段：

```
1. 字符映射（源字符 → 基本源字符集）
2. 行拼接（\ 续行）
3. 预处理（#include、#define 展开）
4. 执行字符集映射
5. 字符串拼接
6. 分词（tokenization）
7. 语法分析 + 语义分析
8. 模板实例化
9. 链接
```
