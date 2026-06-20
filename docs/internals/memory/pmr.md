---
title: "PMR 多态内存资源实现分析"
topic: internals
feature: pmr
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/memory_resource.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory_resource/"
---

# PMR 多态内存资源实现分析

> PMR（Polymorphic Memory Resource）是 C++17 引入的多态内存分配框架，允许在运行时切换内存分配策略。本文基于 GCC 和 LLVM 的源码，分析 PMR 的内部实现。

---

## 一、核心概念

### 1.1 什么是 PMR

PMR 提供了一个抽象的内存资源接口，允许不同的容器共享同一个内存分配策略：

```cpp
// PMR 的基本使用
pmr::monotonic_buffer_resource pool{initial_buffer, sizeof(initial_buffer)};
pmr::vector<int> v{&pool};

// 使用不同的内存资源
pmr::synchronized_pool_resource shared_pool;
pmr::map<string, int> m{&shared_pool};
```

### 1.2 PMR vs 传统分配器

```
PMR vs 传统分配器：

传统分配器：
  · 类型绑定：vector<int, allocator<int>>
  · 编译期确定：每个类型一个分配器
  · 不共享：每个容器独立分配

PMR 分配器：
  · 类型无关：polymorphic_allocator\<byte\>
  · 运行时确定：可以切换内存资源
  · 共享：多个容器共享同一个 memory_resource
```

---

## 二、核心数据结构

### 2.1 memory_resource 接口（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/memory_resource.h:64

class memory_resource {
    static constexpr size_t _S_max_align = alignof(max_align_t);

public:
    memory_resource() = default;
    memory_resource(const memory_resource&) = default;
    virtual ~memory_resource();  // 虚析构函数

    // 分配内存
    [[nodiscard]]
    void* allocate(size_t __bytes, size_t __alignment = _S_max_align) {
        // 调用虚函数 do_allocate 进行实际分配
        return ::operator new(__bytes, do_allocate(__bytes, __alignment));
    }

    // 释放内存
    void deallocate(void* __p, size_t __bytes, size_t __alignment = _S_max_align) {
        // 调用虚函数 do_deallocate 进行实际释放
        return do_deallocate(__p, __bytes, __alignment);
    }

    // 比较两个 memory_resource 是否相等
    [[nodiscard]]
    bool is_equal(const memory_resource& __other) const noexcept {
        return do_is_equal(__other);
    }

private:
    // 子类必须实现的虚函数
    virtual void* do_allocate(size_t __bytes, size_t __alignment) = 0;
    virtual void do_deallocate(void* __p, size_t __bytes, size_t __alignment) = 0;
    virtual bool do_is_equal(const memory_resource& __other) const noexcept = 0;
};

// 比较运算符
[[nodiscard]]
inline bool operator==(const memory_resource& __a, const memory_resource& __b) noexcept {
    return &__a == &__b || __a.is_equal(__b);
}
```

### 2.2 polymorphic_allocator（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/memory_resource.h:122

template<typename _Tp>
class polymorphic_allocator {
public:
    using value_type = _Tp;

    // 默认构造函数：使用默认 memory_resource
    polymorphic_allocator() noexcept {
        extern memory_resource* get_default_resource() noexcept;
        _M_resource = get_default_resource();
    }

    // 指定 memory_resource
    polymorphic_allocator(memory_resource* __r) noexcept
    : _M_resource(__r) { }

    // 分配内存
    [[nodiscard]]
    _Tp* allocate(size_t __n) {
        return static_cast<_Tp*>(_M_resource->allocate(
            __n * sizeof(_Tp), alignof(_Tp)));
    }

    // 释放内存
    void deallocate(_Tp* __p, size_t __n) {
        _M_resource->deallocate(__p, __n * sizeof(_Tp), alignof(_Tp));
    }

    // 获取 memory_resource
    memory_resource* resource() const noexcept { return _M_resource; }

private:
    memory_resource* _M_resource;
};
```

---

## 三、GCC (libstdc++) 的实现

### 3.1 monotonic_buffer_resource

```cpp
// GCC 的单调缓冲区资源
class monotonic_buffer_resource : public memory_resource {
    memory_resource* upstream_;      // 上游资源
    void* buffer_;                   // 当前缓冲区
    size_t buffer_size_;             // 缓冲区大小
    size_t bytes_used_;              // 已使用字节数
    
protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        // 尝试从当前缓冲区分配
        void* ptr = align_up(buffer_ + bytes_used_, alignment);
        if (ptr + bytes <= buffer_ + buffer_size_) {
            bytes_used_ += bytes;
            return ptr;
        }
        // 缓冲区不足，从上游分配新缓冲区
        return allocate_new_buffer(bytes, alignment);
    }
};
```

### 3.2 synchronized_pool_resource

```cpp
// GCC 的同步池资源
class synchronized_pool_resource : public memory_resource {
    // 使用 mutex 保护的内存池
    // 每个大小类一个池
    // 支持多线程安全分配
};
```

---

## 四、LLVM (libc++) 的实现

### 4.1 monotonic_buffer_resource

LLVM 的实现与 GCC 类似：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__memory_resource/monotonic_buffer_resource.h

class monotonic_buffer_resource : public memory_resource {
    struct __chunk {
        __chunk* __next_;
        size_t __size_;
    };
    
    void* __buffer_;
    size_t __buffer_size_;
    size_t __used_;
    __chunk* __chunk_list_;
    memory_resource* __upstream_;
};
```

### 4.2 pool_resource

LLVM 提供了两种池资源：

```cpp
// 非同步池资源
class unsynchronized_pool_resource : public memory_resource {
    // 每个大小类一个池
    // 不支持多线程安全
};

// 同步池资源
class synchronized_pool_resource : public memory_resource {
    // 使用 mutex 保护
    // 支持多线程安全
};
```

---

## 五、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ memory_resource        │ 完整                 │ 完整                 │
│ polymorphic_allocator  │ 完整                 │ 完整                 │
│ monotonic_buffer       │ 完整                 │ 完整                 │
│ synchronized_pool      │ 完整                 │ 完整                 │
│ unsynchronized_pool    │ 无                   │ 有                   │
│ pool_options           │ 支持                 │ 支持                 │
│ is_equal 语义          │ 完整                 │ 完整                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 六、性能特征

```
PMR 的性能：

monotonic_buffer_resource：
  · 分配：O(1)（指针移动）
  · 释放：O(1)（什么都不做）
  · 批量释放：O(1)（重置指针）
  · 最适合：临时分配，批量释放

synchronized_pool_resource：
  · 分配：O(1)（池分配）
  · 释放：O(1)（归还池）
  · 最适合：频繁分配/释放，多线程

unsynchronized_pool_resource：
  · 分配：O(1)（池分配）
  · 释放：O(1)（归还池）
  · 最适合：频繁分配/释放，单线程

与 malloc/free 对比：
  · PMR 更快（避免系统调用）
  · PMR 更少碎片
  · PMR 更适合特定场景
```

---

## 延伸阅读

- [分配器模型](/internals/memory/allocator) — 传统分配器的实现
- [GNU 扩展分配器](/internals/memory/ext-allocators) — pool/bitmap 分配器
- [std::vector 实现](/internals/containers/vector) — vector 与 PMR 的交互
