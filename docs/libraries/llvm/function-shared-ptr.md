# libc++ function 与 shared_ptr

## std::function：24 字节 SBO

### 数据结构

```
__value_func:
  __buf_ (24 字节栈缓冲区)     __f_ (指针)
  ┌─────────────────────────┐  ┌──────┐
  │ callable 对象 或 未使用  │──→│ 指向 │
  └─────────────────────────┘  └──────┘
                               ↑ 可能指向 __buf_（栈）或 堆
```

### SBO 判断

```
  sizeof(_Fun) ≤ 24 AND is_nothrow_copy_constructible ?
    YES → ::new(&__buf_) _Fun(move(lambda))  // 栈上构造
    NO  → new _Fun(lambda)                    // 堆分配
```

### 移动构造的特殊性

```
  src 在栈上？→ 不能偷指针，必须 clone（与拷贝相同）
  src 在堆上？→ 直接偷指针，src.__f_ = nullptr

  关键洞察：即使是 std::function 的移动构造，栈上 callable 也需要拷贝！
```

## std::shared_ptr：控制块布局

### make_shared 单次分配

```
make_shared<int>(42) 的内存布局：

  ┌──────────────────────────────────────┐
  │     __shared_ptr_emplace 控制块       │
  │  vtable ptr (8B)                      │
  │  use_count  (atomic, 4B) = 2          │
  │  weak_count (atomic, 4B) = 1          │
  │  int __elem_ = 42 (4B)               │  ← 紧跟在后面
  └──────────────────────────────────────┘
           ↑ 一次 malloc，整块连续内存

  shared_ptr<int> sp:
  ┌──────────┬──────────┐
  │ _M_ptr ──┼──────────┼──→ __elem_ (42)
  │ _M_ctrl──┼──────────┼──→ 控制块起始
  └──────────┴──────────┘
  sizeof(shared_ptr) = 16 字节（两个指针）
```

### 两级析构

```
use_count → 0: __on_zero_shared()
  → 只析构元素，不释放内存（weak_ptr 还在）

weak_count → 0: __on_zero_shared_weak()
  → 释放控制块 + 元素的整块内存
```
