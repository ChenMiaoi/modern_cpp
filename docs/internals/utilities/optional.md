---
title: "std::optional 实现分析"
topic: internals
feature: optional
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/optional"
source_llvm: "references/impl/llvm-project/libcxx/include/optional"
---

# std::optional 实现分析

> `std::optional` 是 C++17 引入的可选值容器，表示一个可能存在也可能不存在的值。本文基于 GCC 和 LLVM 的源码，分析 optional 的内部实现。

---

## 一、核心概念

### 1.1 什么是 optional

optional 表示一个可能存在也可能不存在的值：

```cpp
optional<int> find_value(const string& key) {
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;  // 有值
    }
    return nullopt;  // 无值
}

// 使用
auto val = find_value("key");
if (val) {
    cout << *val << endl;
} else {
    cout << "not found" << endl;
}
```

### 1.2 optional vs 指针

```
optional vs 指针：

指针：
  · 可能为空
  · 需要手动管理内存
  · 不拥有对象

optional：
  · 可能为空
  · 自动管理内存
  · 拥有对象
  · 类型安全
```

---

## 二、核心数据结构

### 2.1 存储布局

```
optional 的内存布局：

┌─────────────────────────────────────┐
│ bool has_value_                      │  ← 是否有值
├─────────────────────────────────────┤
│ 存储空间（对齐到 T 的对齐）          │
│   ┌─────────────────────────────┐   │
│   │ T value（如果 has_value_）   │   │
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘

sizeof(optional<T>) = sizeof(bool) + sizeof(T) + 填充
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/optional

// GCC 使用 _Optional_payload 存储数据
template<typename _Tp>
class optional {
    // 存储标签
    bool _M_has_value;
    
    // 对齐存储空间
    union {
        aligned_storage_t<sizeof(_Tp), alignof(_Tp)> _M_storage;
    };
    
    // 访问存储的值
    _Tp* _M_ptr() noexcept {
        return reinterpret_cast<_Tp*>(&_M_storage);
    }
    
    const _Tp* _M_ptr() const noexcept {
        return reinterpret_cast<const _Tp*>(&_M_storage);
    }
    
    // 构造值
    template<typename... _Args>
    void _M_construct(_Args&&... __args) {
        ::new (_M_ptr()) _Tp(std::forward<_Args>(__args)...);
        _M_has_value = true;
    }
    
    // 销毁值
    void _M_destroy() {
        _M_ptr()->~_Tp();
        _M_has_value = false;
    }
    
public:
    // 默认构造函数
    constexpr optional() noexcept : _M_has_value(false) {}
    
    // nullopt 构造函数
    constexpr optional(nullopt_t) noexcept : _M_has_value(false) {}
    
    // 值构造函数
    template<typename _Up = _Tp>
    constexpr explicit optional(_Up&& __x) : _M_has_value(true) {
        _M_construct(std::forward<_Up>(__x));
    }
    
    // 拷贝构造函数
    constexpr optional(const optional& __other) : _M_has_value(__other._M_has_value) {
        if (_M_has_value) {
            _M_construct(*__other._M_ptr());
        }
    }
    
    // 移动构造函数
    constexpr optional(optional&& __other) : _M_has_value(__other._M_has_value) {
        if (_M_has_value) {
            _M_construct(std::move(*__other._M_ptr()));
        }
    }
    
    // 析构函数
    ~optional() {
        if (_M_has_value) {
            _M_destroy();
        }
    }
    
    // 检查是否有值
    constexpr bool has_value() const noexcept { return _M_has_value; }
    constexpr explicit operator bool() const noexcept { return _M_has_value; }
    
    // 访问值
    constexpr const _Tp& operator*() const& { return *_M_ptr(); }
    constexpr _Tp& operator*() & { return *_M_ptr(); }
    
    // value_or
    constexpr _Tp value_or(const _Tp& __default) const& {
        return _M_has_value ? *_M_ptr() : __default;
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 存储实现               │ aligned_storage      │ aligned_storage      │
│ 标签                   │ bool                 │ bool                 │
│ constexpr 支持         │ C++20                │ C++20                │
│ value_or               │ 支持                 │ 支持                 │
│ emplace                 │ 支持                 │ 支持                 │
│ swap                   │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
optional 使用指南：

1. 表示可能缺失的值：
   · 函数返回值可能为空
   · 配置项可能未设置

2. 使用 value_or 提供默认值：
   int val = opt.value_or(42);

3. 使用 transform 链式操作：
   auto result = opt.transform([](int x) { return x * 2; });

4. 使用 emplace 原地构造：
   opt.emplace(args...);
```

---

## 延伸阅读

- [std::variant 实现](/internals/utilities/variant) — 标签联合
- [std::any 实现](/internals/utilities/any) — 类型擦除容器
- [std::expected 实现](/internals/cpp23/expected) — C++23 的错误处理
