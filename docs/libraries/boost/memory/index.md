---
title: "Boost 内存管理"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost 内存管理

## SmartPtr

`boost::shared_ptr`、`boost::weak_ptr`、`boost::scoped_ptr`、`boost::intrusive_ptr`——这些是 C++11 智能指针的直接前身。

### intrusive_ptr

与 `shared_ptr` 不同，`intrusive_ptr` 要求被管理对象自身维护引用计数：

```cpp
struct MyObject {
    std::atomic<int> refcount{0};
    // ...
};

void intrusive_ptr_add_ref(MyObject* p) { p->refcount.fetch_add(1); }
void intrusive_ptr_release(MyObject* p) {
    if (p->refcount.fetch_sub(1) == 1) delete p;
}

boost::intrusive_ptr<MyObject> ptr(new MyObject);
```

**优势**：不需要独立的控制块分配（比 `shared_ptr` 少一次 malloc）。

## Pool：内存池

Boost.Pool 提供两种池分配器：

- **`pool<>`**：固定大小块的内存池。分配/释放 O(1)，无碎片。
- **`object_pool<T>`**：对象池，批量构造/析构同类型对象。

```cpp
boost::pool<> alloc(sizeof(int));  // 4 字节块的内存池
int* p = static_cast<int*>(alloc.malloc());  // 从池中分配
alloc.free(p);  // 归还到池中
```

## Align：对齐内存

`boost::alignment::aligned_alloc` 提供对齐内存分配——在 C++17 `std::aligned_alloc` 之前的跨平台方案。
