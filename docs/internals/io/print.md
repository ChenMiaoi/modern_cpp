---
title: "std::print 实现分析"
topic: internals
feature: print
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/print.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__ostream/print.h"
---

# std::print 实现分析

> `std::print` 是 C++23 引入的格式化输出函数，比 std::cout 更快。本文基于 GCC 和 LLVM 的源码，分析 std::print 的内部实现。

---

## 一、核心概念

### 1.1 什么是 std::print

std::print 提供高效的格式化输出：

```cpp
// std::print 的基本使用
print("Hello, {}!\n", "world");
println("Hello, {}!", "world");  // 自动换行

// 格式化输出
print("Pi is {:.4f}\n", 3.14159);
```

---

## 二、GCC vs LLVM 差异对比

### 2.1 GCC 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/print.h

// print 的基本实现
template<typename... _Args>
void print(FILE* __stream, format_string<_Args...> __fmt, _Args&&... __args) {
    // 格式化字符串
    auto __str = std::format(__fmt, std::forward<_Args>(__args)...);
    
    // 写入文件（使用 fwrite_unlocked 提高性能）
    ::fwrite_unlocked(__str.data(), 1, __str.size(), __stream);
}

// println 的实现（自动添加换行）
template<typename... _Args>
void println(FILE* __stream, format_string<_Args...> __fmt, _Args&&... __args) {
    // 格式化字符串
    auto __str = std::format(__fmt, std::forward<_Args>(__args)...);
    
    // 写入文件
    ::fwrite_unlocked(__str.data(), 1, __str.size(), __stream);
    
    // 添加换行
    ::putc_unlocked('\n', __stream);
}

// FILE* 输出的缓冲区管理
class _File_sink final : _Buf_sink<char> {
    struct _File {
        FILE* _M_file;
        
        explicit _File(FILE* __f) : _M_file(__f) {
            ::flockfile(_M_file);  // 加锁
        }
        
        ~_File() { ::funlockfile(_M_file); }  // 解锁
        
        void _M_bump(size_t __n) { _M_file->_IO_write_ptr += __n; }
        char* _M_write_buf() { return _M_file->_IO_write_base; }
        void _M_flush() { ::fflush(_M_file); }
    };
    
    _File _M_file;
    bool _M_add_newline;
    
    void _M_overflow() override {
        auto __s = this->_M_used();
        ::fwrite_unlocked(__s.data(), 1, __s.size(), _M_file._M_file);
        this->_M_reset(_M_file._M_write_buf());
    }
    
public:
    _File_sink(FILE* __f, bool __add_newline)
    : _M_file(__f), _M_add_newline(__add_newline) {
        if (!_M_file._M_unbuffered())
            this->_M_reset(_M_file._M_write_buf());
    }
    
    ~_File_sink() noexcept(false) {
        auto __s = this->_M_used();
        if (__s.data() == this->_M_buf) {
            _File_sink::_M_overflow();
            if (_M_add_newline)
                ::putc_unlocked('\n', _M_file._M_file);
        } else {
            _M_file._M_bump(__s.size());
            if (_M_add_newline)
                ::putc_unlocked('\n', _M_file._M_file);
        }
    }
};
```

### 2.2 对比表

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ print                  │ 支持                 │ 支持                 │
│ println                │ 支持                 │ 支持                 │
│ 缓冲输出               │ 支持                 │ 支持                 │
│ FILE* 输出             │ 支持                 │ 支持                 │
│ ostream 输出           │ 支持                 │ 支持                 │
│ format 集成            │ 支持                 │ 支持                 │
│ fwrite_unlocked        │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::format 实现](/internals/io/format) — format 的实现
- [流类体系](/internals/io/streams) — 传统 I/O 的实现
