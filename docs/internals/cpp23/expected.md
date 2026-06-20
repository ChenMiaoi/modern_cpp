---
title: "std::expected 实现分析"
topic: internals
feature: expected
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/expected"
source_llvm: "references/impl/llvm-project/libcxx/include/__expected/expected.h"
---

# std::expected 实现分析

> `std::expected` 是 C++23 引入的错误处理容器，类似 Rust 的 Result 类型。本文基于 GCC 和 LLVM 的源码，分析 expected 的内部实现。

---

## 一、核心概念

### 1.1 什么是 expected

expected 表示一个可能包含值或错误的容器：

```cpp
// expected 的基本使用
expected<int, error_code> divide(int a, int b) {
    if (b == 0) return unexpected(error_code::divide_by_zero);
    return a / b;
}

auto result = divide(10, 2);
if (result) {
    cout << *result << endl;  // 5
} else {
    cout << "Error: " << result.error() << endl;
}
```

---

## 二、核心数据结构

### 2.1 存储布局

```
expected 的内存布局：

┌─────────────────────────────────────┐
│ bool has_value_                      │  ← 是否有值
├─────────────────────────────────────┤
│ 存储空间                             │
│   ┌─────────────────────────────┐   │
│   │ T value 或 E error          │   │
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/expected

// bad_expected_access 异常类
template<typename _Er>
class bad_expected_access : public bad_expected_access<void> {
public:
    explicit bad_expected_access(_Er __e) : _M_unex(std::move(__e)) { }
    
    // 获取错误值
    _Er& error() & noexcept { return _M_unex; }
    const _Er& error() const & noexcept { return _M_unex; }
    _Er&& error() && noexcept { return std::move(_M_unex); }
    
private:
    _Er _M_unex;  // 存储错误值
};

// unexpected 标签类型
struct unexpect_t {
    explicit unexpect_t() = default;
};
inline constexpr unexpect_t unexpect{};

// unexpected 包装器
template<typename _Er>
class unexpected {
    _Er _M_unex;
public:
    unexpected(const unexpected&) = default;
    unexpected(unexpected&&) = default;
    
    template<typename _Err = _Er>
    explicit unexpected(_Err&& __e) : _M_unex(std::forward<_Err>(__e)) { }
    
    const _Er& error() const& noexcept { return _M_unex; }
    _Er& error() & noexcept { return _M_unex; }
};

// expected 的基本实现
template<typename _Tp, typename _Er>
class expected {
    bool _M_has_value;
    union {
        aligned_storage_t<sizeof(_Tp), alignof(_Tp)> _M_val;
        aligned_storage_t<sizeof(_Er), alignof(_Er)> _M_err;
    };
    
public:
    // 构造函数
    constexpr expected() : _M_has_value(true) {
        ::new (&_M_val) _Tp();
    }
    
    constexpr expected(const expected& __rhs) : _M_has_value(__rhs._M_has_value) {
        if (_M_has_value) {
            ::new (&_M_val) _Tp(__rhs._M_get_val());
        } else {
            ::new (&_M_err) _Er(__rhs._M_get_err());
        }
    }
    
    // 预期值构造函数
    template<typename _Up = _Tp>
    constexpr expected(_Up&& __u) : _M_has_value(true) {
        ::new (&_M_val) _Tp(std::forward<_Up>(__u));
    }
    
    // 意外值构造函数
    template<typename _Err = _Er>
    constexpr expected(unexpected<_Err>&& __e) : _M_has_value(false) {
        ::new (&_M_err) _Er(std::move(__e).error());
    }
    
    // 析构函数
    ~expected() {
        if (_M_has_value) {
            _M_get_val().~_Tp();
        } else {
            _M_get_err().~_Er();
        }
    }
    
    // 检查是否有值
    constexpr bool has_value() const noexcept { return _M_has_value; }
    constexpr explicit operator bool() const noexcept { return _M_has_value; }
    
    // 获取值
    constexpr _Tp& operator*() & { return _M_get_val(); }
    constexpr const _Tp& operator*() const& { return _M_get_val(); }
    constexpr _Tp&& operator*() && { return std::move(_M_get_val()); }
    
    // 获取错误
    constexpr const _Er& error() const& { return _M_get_err(); }
    constexpr _Er& error() & { return _M_get_err(); }
    
    // value_or
    constexpr _Tp value_or(const _Tp& __default) const& {
        return _M_has_value ? _M_get_val() : __default;
    }
    
    // transform
    template<typename _Fn>
    constexpr auto transform(_Fn&& __fn) -> expected<decltype(__fn(declval<_Tp>())), _Er> {
        if (_M_has_value) {
            return expected<decltype(__fn(declval<_Tp>())), _Er>(
                std::forward<_Fn>(__fn)(_M_get_val()));
        }
        return unexpected<(_Er)>(_M_get_err());
    }
    
private:
    _Tp& _M_get_val() { return *reinterpret_cast<_Tp*>(&_M_val); }
    const _Tp& _M_get_val() const { return *reinterpret_cast<const _Tp*>(&_M_val); }
    _Er& _M_get_err() { return *reinterpret_cast<_Er*>(&_M_err); }
    const _Er& _M_get_err() const { return *reinterpret_cast<const _Er*>(&_M_err); }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ expected               │ 支持                 │ 支持                 │
│ unexpected             │ 支持                 │ 支持                 │
│ has_value              │ 支持                 │ 支持                 │
│ value                  │ 支持                 │ 支持                 │
│ error                  │ 支持                 │ 支持                 │
│ transform              │ 支持                 │ 支持                 │
│ and_then               │ 支持                 │ 支持                 │
│ or_else                │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::variant 实现](/internals/utilities/variant) — 标签联合
- [std::optional 实现](/internals/utilities/optional) — 可选值容器
- [std::any 实现](/internals/utilities/any) — 类型擦除容器
