# libc++ (LLVM) 源码级深度剖析

> 本文基于 LLVM 20 的 libc++ 源码，逐行分析 STL 核心组件的实现，配以详细的内存布局图、状态机图、数据流图。

---

## 1. std::vector：三指针布局与 split_buffer

### 1.1 内存布局图

```
vector<int> v = {10, 20, 30};   capacity = 5

  __begin_    __end_         __cap_
    ↓           ↓              ↓
    ┌────┬────┬────┬────────────┐
    │ 10 │ 20 │ 30 │  ?  │  ?  │
    └────┴────┴────┴────────────┘
    ↑              ↑              ↑
    第0个          第2个          第4个
    
    size     = __end_ - __begin_          = 3
    capacity = __cap_  - __begin_          = 5
    剩余     = __cap_  - __end_            = 2
```

三个裸指针（而非 begin + size + capacity），`sizeof(vector)` = 24 字节（64 位，默认空分配器通过 `[[no_unique_address]]` 压缩为 0 字节）。

### 1.2 emplace_back 快慢路径

```
emplace_back(args...) 的控制流：
                        
         ┌──────────────────┐
         │  __end_ < __cap_ │
         └────────┬─────────┘
           ┌──────┴──────┐
       YES ↓              ↓ NO
  ┌────────────────┐  ┌─────────────────────────────┐
  │ 热路径（inline） │  │ 冷路径（__emplace_back_slow） │
  │                │  │                             │
  │ placement new  │  │ 1. __recommend(2*cap)       │
  │ ++__end_       │  │ 2. 分配 split_buffer        │
  │                │  │ 3. 在新缓冲区构造新元素       │
  │ 不生成扩容代码！ │  │ 4. __swap_out_circular_buffer│
  └────────────────┘  └─────────────────────────────┘
```

**`__if_likely_else` 的技巧**（源码 vector.h:1108）：

```cpp
void __if_likely_else(bool __cond, _If __if, _Else __else) {
  if (__builtin_constant_p(__cond)) {
    // 编译期已知条件 → 直接消除未走分支的代码
    if (__cond) __if(); else __else();
  } else {
    if (__cond) [[__likely__]] __if();  // 标记 likely → 分支预测
    else __else();
  }
}
```

为什么需要这个函数？LLVM 的一个 bug（PR 154292）：当 `if` 标记 `[[unlikely]]`，即使条件编译期已知为 false，编译器也不会内联 `else` 分支。`__builtin_constant_p` 绕过了这个问题。

### 1.3 增长策略：2 倍翻倍

```
__recommend 源码（vector.h:901）：

新容量 = max(2 × 当前capacity, 请求的新size)

示例：capacity=4, push_back → recommend(5) → max(8, 5) = 8

增长过程：
  [A B C D]           capacity=4
  → 分配 8 个位置的新缓冲区
  → 重定位 A B C D 到新缓冲区
  → 构造新元素 E
  → 释放旧缓冲区
  
  [A B C D E ? ? ?]   capacity=8
```

### 1.4 split_buffer：中间有洞的缓冲区

insert 在中间位置插入时，libc++ 使用 split_buffer：

```
原始 vector: [A B C D E]   capacity=5, insert(2, X)

Step 1: 分配 split_buffer（capacity=10，在位置 2 处留空）
  split_buffer:
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │   │   │空 │   │   │   │   │   │   │   │
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
              ↑ __begin()
                ↑ 空位（新元素插入点）
                       ↑ __end()

Step 2: 先重定位 [D, E] 到 split_buffer 末尾
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │   │   │空 │   │   │   │   │ D │ E │   │
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

Step 3: 再重定位 [A, B] 到 split_buffer 开头
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │ A │ B │空 │   │   │   │   │ D │ E │   │
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

Step 4: 在空位放置新元素 X，交换三指针
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │ A │ B │ X │   │   │   │   │ D │ E │   │
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
              ↑ 新 begin                        ↑ 新 end

Step 5: 旧 vector 的三指针被替换，旧缓冲区释放
```

**为什么先重定位后半段？** 异常安全。如果 `[A,B)` 中某个元素的 move 构造抛异常，`[D,E)` 已经安全地在新位置了——异常处理器可以清理两边。

### 1.5 `__uninitialized_allocator_relocate`：memcpy 优化

```
五重条件判断（uninitialized_algorithms.h:553）：

  ┌─────────────────────────────────────────────────────┐
  │ 类型 trivially relocatable？                         │
  │   AND 分配器的 move construct 是平凡的？              │
  │   AND 分配器的 destroy 是平凡的？                     │
  │   AND 不在 constexpr 上下文？                        │
  └──────────────────────┬──────────────────────────────┘
               ┌─────────┴──────────┐
           ALL YES               ANY NO
               ↓                    ↓
  ┌────────────────────┐  ┌──────────────────────────┐
  │ __builtin_memcpy   │  │ 逐个 move_if_noexcept +  │
  │ 一次搬移整个区间    │  │ destroy，带异常安全 guard │
  │                    │  │                          │
  │ 适用于：           │  │ 适用于：                  │
  │ unique_ptr         │  │ 自定义类型（非平凡）      │
  │ shared_ptr         │  │                          │
  │ 原始指针           │  │                          │
  └────────────────────┘  └──────────────────────────┘
```

---

## 2. std::string：24 字节 SSO 布局

### 2.1 字节级内存图

```
sizeof(basic_string) = 24 字节（64 位 libc++）

┌──────────────────────────────────────────────────────────┐
│                  24 字节 raw bytes                        │
└──────────────────────────────────────────────────────────┘

Short 模式（≤ 22 字节）：最后字节最低位 = 0

  字节 0    字节 1-22   字节 23
  ┌──────┬──────────────┬────────────────────┐
  │ 0x00 │ "hello world"│ (23-size)<<1 | 0   │ ← 最低位=0 → Short
  └──────┴──────────────┴────────────────────┘
  存储内容：字符串数据      size 编码：23-11=12 → 0x18
  
  示例 s = "hello" (5字节)：
  字节 0-4:  'h' 'e' 'l' 'l' 'o'
  字节 5:    '\0'
  字节 6-22: 未初始化（垃圾）
  字节 23:   (23-5)<<1 = 0x24 → 二进制 00100100 → 最低位=0 → Short！

Long 模式（> 22 字节）：最后字节最低位 = 1

  字节 0-7      字节 8-15    字节 16-23
  ┌────────────┬────────────┬────────────┐
  │ capacity|1 │   size     │  data*     │ ← 最低位=1 → Long
  └────────────┴────────────┴────────────┘
  容量（奇数）   当前长度      堆分配指针
```

**为什么 22 字节？** `sizeof(__long)` = 8+8+8 = 24。短模式的 `__data_[23]` 占 23 字节，减去 1 字节 `\0` = 22。当 size=22 时，`small_[22] = '\0'`，字节 23 = `(23-22)<<1 = 0x02`，最低位=0 → Short 模式，恰好填满所有 23 个数据字节。

### 2.2 模式判断的巧妙之处

```cpp
// 源码（简化）：检查字节 23 的最低位
Category category() const {
  return static_cast<Category>(bytes_[23] & 0xC0);  // 提取高2位
  // 0x00 = Small, 0x80 = Medium(旧), 0x40 = Large(旧)
}

// libc++ 的方案更简单：检查最低位
// small_[maxSmallSize] 的最低位：0=Small, 1=Long
```

**对比 libstdc++**（32 字节，SSO 15）和 MSVC（32 字节，SSO 15）：

```
libc++ (24B):     [22B内联][1B size+tag]     SSO = 22
libstdc++ (32B):  [ptr 8B][len 8B][union 16B] SSO = 15  
MSVC (32B):       [union 16B][len 8B][cap 8B] SSO = 15

存储百万个短字符串时，libc++ 比其他实现少用 ~25% 内存。
```

---

## 3. std::function：24 字节 SBO 的完整生命周期

### 3.1 数据结构

```
__value_func 的内存布局：

  __buf_ (24 字节栈缓冲区)          __f_ (指针)
  ┌─────────────────────────────┐   ┌──────┐
  │  callable 对象 或 未使用     │──→│ 指向 │
  └─────────────────────────────┘   └──────┘
                                    ↑
                              可能指向 __buf_（栈上）
                              也可能指向堆（new 分配）
```

### 3.2 构造时的 SBO 判断

```
构造 __value_func(lambda) 的决策树：

  sizeof(_Fun) ≤ 24  AND  is_nothrow_copy_constructible
         ┌──────────────┴──────────────┐
         ↓ YES                         ↓ NO
  ┌──────────────────┐      ┌────────────────────┐
  │ ::new(&__buf_)   │      │ new _Fun(lambda)   │
  │ _Fun(move(lambda))│      │                    │
  │                  │      │ __f_ 指向堆内存     │
  │ __f_ = &__buf_   │      └────────────────────┘
  └──────────────────┘

  _Fun 大小 = vtable指针(8) + sizeof(lambda) + padding
  一个捕获2个指针的lambda(16B) → _Fun=24B → 恰好进入SBO
  一个捕获3个指针的lambda(24B) → _Fun=32B → 堆分配
```

### 3.3 拷贝构造：指针比较区分栈/堆

```
拷贝构造 __value_func(src) 的决策树：

  src.__f_ == nullptr ?
  ┌──────────┴──────────┐
  ↓ YES                  ↓ NO
  __f_ = nullptr    (void*)src.__f_ == &src.__buf_ ?
                    ┌──────┴──────┐
                    ↓ YES          ↓ NO
           ┌───────────────┐  ┌────────────────┐
           │ src 在栈上    │  │ src 在堆上     │
           │ __f_ = &__buf_│  │ __f_ = src.__f_│
           │ __clone(__f_) │  │   ->__clone()  │
           │ (就地拷贝)    │  │ (new 新副本)   │
           └───────────────┘  └────────────────┘
```

### 3.4 移动构造：栈上仍然需要拷贝

```
移动构造 __value_func(src) 的决策树：

  (void*)src.__f_ == &src.__buf_ ?
  ┌──────────┴──────────┐
  ↓ YES                  ↓ NO
  src 在栈上             src 在堆上
  ┌─────────────────┐    ┌─────────────────┐
  │ 不能偷指针！     │    │ 直接偷指针：    │
  │ 必须 clone       │    │ __f_ = src.__f_ │
  │ (与拷贝相同)     │    │ src.__f_ = null │
  └─────────────────┘    └─────────────────┘

关键洞察：即使是 std::function 的移动构造，
栈上的 callable 也需要拷贝！比直接移动慢。
```

### 3.5 析构：两种释放路径

```
~__value_func() 的决策树：

  __f_ 指向哪里？
  ┌───────────┴───────────┐
  ↓ &__buf_               ↓ 堆 或 nullptr
  ┌─────────────────┐     ┌─────────────────────┐
  │ __f_->destroy() │     │ if (__f_)           │
  │ 只调用析构函数   │     │   __f_->destroy_    │
  │ 不释放内存       │     │     deallocate();   │
  │ (栈内存自动释放) │     │ 析构 + delete       │
  └─────────────────┘     └─────────────────────┘
```

---

## 4. std::shared_ptr：控制块布局

### 4.1 make_shared 的单次分配布局

```
make_shared<int>(42) 的内存布局：

  ┌──────────────────────────────────────┐
  │     __shared_ptr_emplace 控制块       │
  │  ┌─────────────────────────────────┐ │
  │  │ vtable ptr (8B)                 │ │
  │  │ __shared_weak_count base:       │ │
  │  │   use_count  (atomic, 4B)       │ │
  │  │   weak_count (atomic, 4B)       │ │
  │  ├─────────────────────────────────┤ │
  │  │ _Storage:                       │ │
  │  │   allocator (0B, 空类型)         │ │
  │  │   int __elem_ = 42 (4B)         │ │ ← 紧跟在 allocator 后面
  │  └─────────────────────────────────┘ │
  └──────────────────────────────────────┘
           ↑ 一次 malloc，整块连续内存
           
  shared_ptr<int> sp:
  ┌──────────┬──────────┐
  │ _M_ptr───┼──────────┼──→ __elem_ (42)
  │ _M_ctrl──┼──────────┼──→ 控制块起始
  └──────────┴──────────┘
  sizeof(shared_ptr) = 16 字节（两个指针）
```

### 4.2 shared_ptr(new T) 的两次分配

```
shared_ptr<int>(new int(42)) 的内存布局：

  第一次分配（new int）：     第二次分配（控制块）：
  ┌──────────┐               ┌──────────────────────────┐
  │ int = 42 │               │ vtable ptr               │
  └──────────┘               │ use_count = 1            │
       ↑                     │ weak_count = 1           │
       │                     │ __ptr_ ──────────────────┼──→ 42
  对象独立堆分配              │ __deleter_ (default_del) │
                             │ __alloc_ (空)            │
                             └──────────────────────────┘
                                      ↑
                              控制块单独堆分配
```

### 4.3 两级析构流程

```
use_count → 0 时（最后一个 shared_ptr 销毁）：

  __on_zero_shared() {
    // 只析构元素，不释放内存！
    // 因为 weak_ptr 还在，需要能检查 use_count
    allocator_traits::destroy(__get_elem());  // ~int()
  }

  此时内存状态：
  ┌──────────────────────────────┐
  │ 控制块仍然存活               │
  │ use_count = 0                │
  │ weak_count = 1（weak_ptr还在）│
  │ 元素已析构（未定义值）       │
  └──────────────────────────────┘

weak_count → 0 时（最后一个 weak_ptr 销毁）：

  __on_zero_shared_weak() {
    // 释放控制块 + 元素的整块内存
    allocator_traits::deallocate(__tmp, this, 1);
  }
```

---

## 5. std::map/set：红黑树

### 5.1 节点结构

```
__tree_node 的内存布局：

  ┌────────────────────────────────────┐
  │  __left_    (8B)                   │
  │  __right_   (8B)                   │
  │  __parent_  (8B)                   │
  │  __is_black_ (1B + 7B padding)     │
  ├────────────────────────────────────┤
  │  value_type (T)                    │  ← 数据存在节点末尾
  └────────────────────────────────────┘
  每个节点 = 32B + sizeof(T)
```

### 5.2 __end_node_ 哨兵技巧

```
__tree 的内存布局：

  ┌──────────────────────────────────────┐
  │ __begin_node_ → 最左节点（begin()）  │
  │ __end_node_:                         │
  │   __left_ → 根节点 root              │
  │   __right_ → 最右节点（--end()）     │
  │   __is_black_ = true                 │
  │ __size_ = N                          │
  └──────────────────────────────────────┘

  树结构示意：
  
              __end_node_
              ┌──────────┐
              │ __left_──┼──┐
              └──────────┘  │
                   ↑        ↓
                   │     ┌──────┐
                   │     │ root │ ← __parent_ → __end_node_
                   │     └──┬───┘
                   │    ┌───┴───┐
                   │  ┌─┴─┐  ┌─┴─┐
                   │  │ L │  │ R │
                   │  └───┘  └───┘
                   │
  __begin_node_ ───┘ (最左节点)
  
  end() → &__end_node_
  
  迭代器 ++it 的算法：
  if (__right_) → __tree_min(__right_)    // 右子树最左
  else          → 向上走到"我是左子树"的祖先 → 返回祖先的 parent
```

### 5.3 左旋操作图解

```
__tree_left_rotate(x):

  旋转前:          旋转后:
       x                y
      / \              / \
     A   y            x   C
        / \          / \
       B   C        A   B

  步骤：
  1. y = x.__right_
  2. x.__right_ = y.__left_     // B 成为 x 的右孩子
  3. y.__left_ = x               // x 成为 y 的左孩子
  4. y.__parent_ = x.__parent_   // y 接管 x 的父节点
  5. 更新 x.__parent_ 的左/右指针指向 y
```

### 5.4 插入后重平衡（三种情况）

```
Case 1: 叔叔是红色 → 变色
  变色前:            变色后:
     G(黑)              G(红)        ← 爷爷变红
    / \                / \
  P(红) U(红)       P(黑) U(黑)    ← 父、叔变黑
  /                    /
X(红)                X(红)          ← 继续向上检查

Case 2: X 是右孩子 → 左旋父节点 → 变成 Case 3
  旋转前:            旋转后:
     G(黑)              G(黑)
    / \                / \
  P(红) U(黑)       X(红) U(黑)
    \               /
     X(红)         P(红)

Case 3: X 是左孩子 → 变色 + 右旋祖父
  操作前:            操作后:
     G(黑)              P(黑)        ← 父变黑
    / \                / \
  P(红) U(黑)       X(红) G(红)    ← 爷变红
  /                        \
X(红)                      U(黑)

  最多旋转 2 次（Case 2 + Case 3 各一次）
  变色可能传播到根（O(log n) 次），但旋转是 O(1)
```

### 5.5 删除操作

```
删除节点 z 的决策树：

  z 有几个子树？
  ┌─────────┼─────────┐
  ↓ 0       ↓ 1       ↓ 2
  直接删除   用子树     找中序后继 y
             替换 z     交换 z↔y 的位置
                        （y 最多 1 个子树）
                        → 回到 0 或 1 子树情况

  如果被删节点是黑色 → 黑高减少 1 → "双重黑色"修正
  最多 O(log n) 次旋转/变色
```

---

## 6. std::variant：函数指针表 visit

### 6.1 visit 的实现策略

```
variant<int, string, double> v = 42;

visit(visitor, v) 的内部：

  编译期生成函数指针表：
  ┌────────────────────────────────────────┐
  │ __table[0] = [](vis, v) {              │
  │   return vis(get<int>(v));             │  ← index=0: int
  │ };                                     │
  │ __table[1] = [](vis, v) {              │
  │   return vis(get<string>(v));          │  ← index=1: string
  │ };                                     │
  │ __table[2] = [](vis, v) {              │
  │   return vis(get<double>(v));          │  ← index=2: double
  │ };                                     │
  └────────────────────────────────────────┘

  运行时：__table[v.index()](visitor, v)
                  ↑
            v.index() = 0 → 调用 __table[0] → visitor(42)

  多 variant visit：visit(f, v1, v2)
  v1 有 N 种类型, v2 有 M 种类型 → N×M 个函数指针
```

---

## 7. Ranges：管道运算符的 CRTP + compose

### 7.1 `operator|` 的两个重载

```
重载 1: range | closure
  vec | views::filter(pred)
  ↓
  operator|(vec, filter_closure)
  = std::invoke(filter_closure, vec)    // filter_closure(vec)
  = filter_view(vec, pred)

重载 2: closure | closure
  views::filter(pred) | views::transform(fn)
  ↓
  operator|(filter_closure, transform_closure)
  = __pipeable(__compose(transform_closure, filter_closure))
  // __compose(g, f)(x) = g(f(x))
  // 调用时: transform(filter(vec))
```

### 7.2 partial application

```
views::filter(pred) 的展开：

  views::filter(pred)
  = __pipeable(__bind_back(filter_fn, pred))
  // __bind_back(fn, arg) 返回一个闭包：
  //   closure(range) = fn(range, pred)

  当 vec | closure 时：
  = closure(vec)
  = __bind_back(filter_fn, pred)(vec)
  = filter_fn(vec, pred)
  = filter_view(vec, pred)
```

### 7.3 filter_view 的 begin() 缓存

```
filter_view 内部布局：

  ┌─────────────────────────────────────────────┐
  │ __base_  (原始 view)                        │
  │ __pred_  (谓词函数)                         │
  │ __cached_begin_ (缓存的 begin 迭代器)       │
  │   仅对 forward_range 启用缓存               │
  └─────────────────────────────────────────────┘

  begin() 调用流程：
  if (缓存有效) → 直接返回缓存迭代器
  else          → ranges::find_if(__base_, __pred_)
                  → 缓存结果 → 返回
                  
  __non_propagating_cache: 移动 filter_view 时缓存清空
  → 防止旧迭代器在新 view 中悬空
```

---

## 8. std::unique_ptr：compressed_pair 与 trivial_abi

```
sizeof(unique_ptr<T, default_delete<T>>) = 8 字节（64 位）

  _LIBCPP_COMPRESSED_PAIR 展开（Clang）：
  ┌──────────────────────────────────┐
  │ [[no_unique_address]] T* __ptr_  │  ← 8 字节
  │ [[no_unique_address]] default_   │  ← 0 字节（空类型）
  │            delete<T> __deleter_  │
  └──────────────────────────────────┘
  sizeof = 8

  _LIBCPP_COMPRESSED_PAIR 展开（GCC）：
  ┌──────────────────────────────────┐
  │ __attribute__((aligned)) T* ptr  │  ← 8 字节
  │ padding (if needed)              │
  │ default_delete<T> deleter        │  ← 0 字节
  │ padding                          │
  └──────────────────────────────────┘
```

---

## 三实现综合对比

| 组件 | libc++ (LLVM) | libstdc++ (GCC) | MSVC STL |
|------|--------------|-----------------|----------|
| sizeof(string) | **24** | 32 | 32 |
| string SSO | **22** | 15 | 15 |
| vector 增长 | 2× | ~2× | 1.5× |
| vector relocate | **memcpy** | move+destroy | move+destroy |
| unique_ptr | 8B | 8B | 8B |
| trivial_abi | **支持** | 不支持 | 不支持 |
| shared_ptr make | 1 次 malloc | 1 次 | 1 次 |
| function SBO | 24B | 24B | 不同 |
| map/set | __tree (RB) | _Rb_tree (RB) | RB-tree |
| unordered_map | 链式 | **SwissTable** | 链式 |
| Ranges | **最早完整** | 渐进补齐 | 较晚开始 |
| format 浮点 | 中等 | 中等 | **最好** |
| ABI 稳定性 | 版本化 | **最强** | 强 |

**选 libc++**：macOS/iOS/Android、最紧凑内存布局、trivially relocatable memcpy、最新 C++ 标准。

**选 libstdc++**：Linux 服务器、最强 ABI 兼容、内置 SwissTable。

**选 MSVC STL**：Windows、最好 `std::format` 浮点性能。
