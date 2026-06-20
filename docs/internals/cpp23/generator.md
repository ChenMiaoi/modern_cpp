---
title: "std::generator 实现分析"
topic: internals
feature: generator
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/generator"
source_llvm: "N/A"
---

# std::generator 实现分析

> `std::generator` 是 C++23 引入的协程类型，用于生成惰性序列。本文基于 GCC 的源码，分析 generator 的内部实现。

---

## 一、核心概念

### 1.1 什么是 generator

generator 是一个协程类型，用于惰性生成值序列：

```cpp
// generator 的基本使用
generator<int> fib() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}

// 使用
for (auto val : fib()) {
    cout << val << endl;
    if (val > 100) break;
}
```

---

## 二、核心数据结构

### 2.1 协程帧

```
generator 的协程帧布局：

┌─────────────────────────────────────┐
│ vptr（虚函数表指针）                 │
├─────────────────────────────────────┤
│ promise 对象                        │
│   ├── 当前值                        │
│   └── 暂停状态                      │
├─────────────────────────────────────┤
│ 局部变量存储                        │
└─────────────────────────────────────┘
```

### 2.2 promise_type 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/generator

// generator 的 promise_type
template<typename _Tp, typename _Alloc = allocator<_Tp>>
struct generator_promise {
    // 存储当前值
    _Tp _M_value;
    
    // 返回协程句柄
    generator<_Tp, _Alloc> get_return_object() {
        return generator<_Tp, _Alloc>(
            coroutine_handle<generator_promise>::from_promise(*this));
    }
    
    // 初始暂停：从不暂停
    suspend_always initial_suspend() noexcept { return {}; }
    
    // 最终暂停：总是暂停
    suspend_always final_suspend() noexcept { return {}; }
    
    // co_yield：存储值并暂停
    suspend_always yield_value(const _Tp& __value) noexcept {
        _M_value = __value;
        return {};  // 暂停，等待恢复
    }
    
    suspend_always yield_value(_Tp&& __value) noexcept {
        _M_value = std::move(__value);
        return {};
    }
    
    // 返回 void
    void return_void() {}
    
    // 异常处理
    void unhandled_exception() {
        std::terminate();
    }
};

// generator 类
template<typename _Tp, typename _Alloc = allocator<_Tp>>
class generator {
    using _Promise = generator_promise<_Tp, _Alloc>;
    using _Handle = coroutine_handle<_Promise>;
    
    _Handle _M_handle;
    
public:
    // 迭代器类
    class iterator {
        _Handle _M_handle;
        
    public:
        using value_type = _Tp;
        using difference_type = ptrdiff_t;
        using iterator_category = input_iterator_tag;
        
        iterator() = default;
        explicit iterator(_Handle __h) : _M_handle(__h) {}
        
        const _Tp& operator*() const {
            return _M_handle.promise()._M_value;
        }
        
        iterator& operator++() {
            _M_handle.resume();  // 恢复协程
            return *this;
        }
        
        iterator operator++(int) {
            iterator __tmp = *this;
            ++*this;
            return __tmp;
        }
        
        bool operator==(const iterator& __other) const {
            return _M_handle == __other._M_handle;
        }
        
        bool operator!=(const iterator& __other) const {
            return !(*this == __other);
        }
    };
    
    // begin/end
    iterator begin() {
        if (_M_handle) {
            _M_handle.resume();  // 恢复到第一个 co_yield
        }
        return iterator(_M_handle);
    }
    
    iterator end() {
        return iterator();  // 空迭代器表示结束
    }
    
    // 构造/析构
    explicit generator(_Handle __h) : _M_handle(__h) {}
    ~generator() {
        if (_M_handle) {
            _M_handle.destroy();
        }
    }
    
    generator(generator&& __other) : _M_handle(__other._M_handle) {
        __other._M_handle = nullptr;
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ generator              │ 支持                 │ 实验中               │
│ co_yield               │ 支持                 │ 支持                 │
│ 惰性求值               │ 支持                 │ 支持                 │
│ 无栈协程               │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [协程 Lowering](/internals/cpp20/coroutines) — 协程的编译实现
- [Ranges 框架](/internals/algorithms/ranges) — generator 与 ranges 的交互
