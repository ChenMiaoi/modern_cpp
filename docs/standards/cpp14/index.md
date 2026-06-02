---
title: "C++14"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++14

C++14（ISO/IEC 14882:2014）是 C++11 的增量更新，主要完成 C++11 中"未竟之业"，而非引入全新的编程范式。

## 核心特性

| 特性 | 说明 |
|------|------|
| 泛型 Lambda | Lambda 参数可使用 `auto` |
| 返回类型推导 | 函数可省略尾置返回类型 |
| 变量模板 | `template<typename T> constexpr T pi = T(3.14159...);` |
| 二进制字面量 | `0b1010` 语法 |
| 数字分隔符 | `1'000'000` 提升可读性 |
| `std::make_unique` | 补全 C++11 遗漏的工厂函数 |
| `std::exchange` | 通用交换操作 |
| `std::shared_timed_mutex` | 带超时的共享互斥锁 |

## 定位

C++14 的使命是让 C++11 的设计更加好用。泛型 Lambda 和返回类型推导使得"写泛型代码"变得更加简洁，但没有改变 C++11 建立的基本范式。

## 编译器支持

| 编译器 | 完整支持版本 |
|--------|-------------|
| GCC | 5.1+ |
| Clang | 3.4+ |
| MSVC | VS 2017 (15.0)+ |

## 延伸阅读

- [泛型 Lambda](/standards/cpp14/generic-lambda)
- [返回类型推导](/standards/cpp14/return-type-deduction)
- [变量模板](/standards/cpp14/variable-templates)
