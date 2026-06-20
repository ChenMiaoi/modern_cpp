---
title: "std::format 实现分析"
topic: internals
feature: format
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/formatfwd.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__format/"
---

# std::format 实现分析

> `std::format` 是 C++20 引入的类型安全格式化函数，基于 fmtlib 设计。本文基于 GCC 和 LLVM 的源码，分析 std::format 的内部实现。

---

## 一、核心概念

### 1.1 什么是 std::format

std::format 提供类型安全的字符串格式化：

```cpp
// std::format 的基本使用
string s = format("Hello, {}! You are {} years old.", "Alice", 30);

// 格式化数字
string s2 = format("Pi is {:.4f}", 3.14159);

// 格式化十六进制
string s3 = format("0x{:X}", 255);
```

---

## 二、核心数据结构

### 2.1 编译期格式解析（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/format

// 编译期解析格式字符串
template<typename CharT, typename... Args>
consteval auto make_format_string(const basic_string_view<CharT> str) {
    // 编译期解析格式字符串
    // 检查参数类型是否匹配
    return basic_format_string<CharT, type_identity_t<Args>...>{str};
}

// basic_format_string 的实现
template<typename CharT, typename... Args>
class basic_format_string {
    basic_string_view<CharT> str_;
    
public:
    // 构造函数：编译期验证格式字符串
    template<typename _S>
    consteval basic_format_string(const _S& __s) : str_(__s) {
        // 编译期检查格式字符串中的参数类型是否匹配
        // 如果不匹配，编译失败
        using _Check = basic_format_parse_context<CharT>;
        auto __it = str_.begin();
        auto __end = str_.end();
        // 解析每个 {}
        while (__it != __end) {
            if (*__it == '{') {
                // 解析格式说明符
                ++__it;
                // ... 检查参数索引和类型
                ++__it;
            }
        }
    }
    
    // 获取格式字符串
    basic_string_view<CharT> get() const noexcept { return str_; }
};
```

### 2.2 formatter 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/format

// 整数类型的 formatter
template<typename _Tp, typename CharT>
struct formatter<_Tp, CharT,
    enable_if_t<conjunction_v<is_integral<_Tp>, negation_t<is_same<_Tp, bool>>,
                                negation_t<is_same<_Tp, CharT>>>>> {
    // 格式说明符
    int base_ = 10;        // 进制
    bool fill_zero_ = false;  // 是否用 0 填充
    int width_ = 0;        // 宽度
    int precision_ = -1;   // 精度
    
    // 解析格式说明符
    consteval auto parse(format_parse_context& ctx) {
        auto __it = ctx.begin();
        auto __end = ctx.end();
        
        // 解析填充字符和对齐
        if (__it != __end && *__it != '}') {
            if (*__it == '0') {
                fill_zero_ = true;
                ++__it;
            }
        }
        
        // 解析宽度
        while (__it != __end && is_digit(*__it)) {
            width_ = width_ * 10 + (*__it - '0');
            ++__it;
        }
        
        // 解析类型
        if (__it != __end) {
            switch (*__it) {
                case 'd': case 'i': base_ = 10; break;
                case 'x': base_ = 16; break;
                case 'X': base_ = 16; break;
                case 'o': base_ = 8; break;
                case 'b': base_ = 2; break;
            }
        }
        
        return __it;
    }
    
    // 格式化值
    auto format(const _Tp& value, format_context& ctx) const {
        auto __out = ctx.out();
        
        // 根据进制格式化
        if (base_ == 16) {
            // 十六进制
            __out = format_to(__out, "{:X}", value);
        } else if (base_ == 8) {
            // 八进制
            __out = format_to(__out, "{:o}", value);
        } else {
            // 十进制
            __out = format_to(__out, "{}", value);
        }
        
        return __out;
    }
};
```

### 2.3 format_context 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/format

// format_context 提供格式化上下文
template<typename OutputIt, typename CharT>
class basic_format_context {
    OutputIt out_;
    basic_format_args<basic_format_context> args_;
    locale loc_;
    
public:
    using iterator = OutputIt;
    using char_type = CharT;
    using format_arg = basic_format_arg<basic_format_context>;
    
    // 获取输出迭代器
    iterator out() { return out_; }
    
    // 获取参数
    format_arg arg(size_t __id) {
        return args_.get(__id);
    }
    
    // 获取 locale
    const locale& locale() { return loc_; }
};

// format_args 存储格式化参数
template<typename Context>
class basic_format_args {
    basic_format_arg<Context> args_[max_packed_args];
    size_t size_;
    
public:
    // 获取参数
    basic_format_arg<Context> get(size_t __id) const {
        if (__id < max_packed_args) {
            return args_[__id];
        }
        return basic_format_arg<Context>{};
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 编译期解析             │ 支持                 │ 支持                 │
│ formatter 特化         │ 完整                 │ 完整                 │
│ Unicode 支持           │ 部分                 │ 完整                 │
│ format_to              │ 支持                 │ 支持                 │
│ format_to_n            │ 支持                 │ 支持                 │
│ vformat                │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::print 实现](/internals/io/print) — print 的实现
- [流类体系](/internals/io/streams) — 传统 I/O 的实现
- [fmt 格式化引擎](/libraries/fmt-spdlog/fmt-engine) — fmtlib 的实现
