---
title: libc++ vector 与 string
topic: libraries
feature: vector-string
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/vector
      - references/impl/llvm-project/libcxx/include/string
    symbols:
      - std::vector
      - _SplitBuffer
      - __recommend
      - std::basic_string
      - __is_long
exercises: []
solutions: []
---
# libc++ vector 与 string

> 源码路径：`references/impl/llvm-project/libcxx/include/vector`, `string`

## std::vector：三指针布局与 split_buffer

### 内存布局

```
vector<int> v = {10, 20, 30};   capacity = 5

  __begin_    __end_         __cap_
    ↓           ↓              ↓
    ┌────┬────┬────┬────────────┐
    │ 10 │ 20 │ 30 │  ?  │  ?  │
    └────┴────┴────┴────────────┘

    size     = __end_ - __begin_  = 3
    capacity = __cap_  - __begin_  = 5
    sizeof(vector) = 24 字节（3 个裸指针，空分配器通过 [[no_unique_address]] 压缩为 0）
```

### emplace_back 快慢路径

```
         ┌──────────────────┐
         │  __end_ < __cap_ │
         └────────┬─────────┘
           ┌──────┴──────┐
       YES ↓              ↓ NO
  ┌────────────────┐  ┌─────────────────────────────┐
  │ 热路径（inline） │  │ 冷路径（__emplace_back_slow） │
  │ placement new  │  │ 1. __recommend(2×cap)        │
  │ ++__end_       │  │ 2. 分配 split_buffer         │
  │                │  │ 3. __swap_out_circular_buffer │
  └────────────────┘  └─────────────────────────────┘
```

**`__if_likely_else` 技巧**（vector.h:1108）：当条件编译期已知时直接消除未走分支，否则标记 `[[likely]]` 优化分支预测。

### split_buffer：中间有洞的缓冲区

```
原始: [A B C D E] capacity=5, insert(2, X)

Step 1: 分配 split_buffer(capacity=10, 位置 2 留空)
Step 2: 先重定位 [D, E] 到末尾 → [? ? ? ? ? ? ? D E ?]
Step 3: 再重定位 [A, B] 到开头 → [A B ? ? ? ? ? D E ?]
Step 4: 在空位放置 X           → [A B X ? ? ? ? D E ?]

为什么先重定位后半段？异常安全。
```

### memcpy 优化

```cpp
// 五重条件：trivially relocatable AND 平凡 move/destroy AND 非 constexpr
// ALL YES → __builtin_memcpy 一次搬移整个区间
// ANY NO  → 逐个 move_if_noexcept + destroy
```

## std::string：24 字节 SSO

> **注意**：libc++ 有两套 string ABI 布局（default 和 alternate）。以下描述的是 **alternate layout**（`_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT`），也是源码中的默认实现。`_LIBCPP_ABI_STRING_PAIR_LAYOUT` 等其他 ABI 选项的布局不同。

```
sizeof(basic_string) = 24 字节

Short 模式（≤ 22 字节）：最后字节最低位 = 0
  字节 0-22: 字符数据（最多 22 字节 + \0）
  字节 23:   (23-size)<<1 | 0

Long 模式（> 22 字节）：最后字节最低位 = 1
  字节 0-7:   capacity|1 (奇数)
  字节 8-15:  size
  字节 16-23: data* (堆分配指针)

SSO 容量 = 22 字节 → 存储百万个短字符串时比其他实现少用 ~25% 内存
```

**为什么 22 字节？** `sizeof(__long)` = 8+8+8 = 24。短模式的 `__data_[23]` 占 23 字节，减去 1 字节 `\0` = 22。

**`__is_long()` 如何判断？** 检查最后字节的最低位：`0` = Short，`1` = Long。这个判断在几乎所有 string 操作的热路径上执行。

## 标准语义

### vector 标准要求

| 要求 | 条款 | libc++ 实现方式 |
|---|---|---|
| 连续存储 | \[vector.overview\] | `__begin_` 到 `__end_` 连续内存，`data()` 返回 `__begin_` |
| `operator[]` 不做边界检查 | \[vector.access\] | 直接解引用 `*(__begin_ + n)`，UB 越界 |
| `at()` 做边界检查 | \[vector.access\] | 越界抛 `std::out_of_range` |
| `push_back` 强异常保证 | \[vector.modifiers\] | 通过 split_buffer 实现：先分配新缓冲区再搬移，失败则原数据不变 |
| `insert` 强异常保证 | \[vector.modifiers\] | 同上，split_buffer 中间留空策略保证异常安全 |
| 移动构造 noexcept | \[vector.cons\] | `noexcept(is_nothrow_move_constructible<allocator_type>::value)` |
| `reserve` 不使迭代器失效（若 n ≤ capacity） | \[vector.capacity\] | 仅当 `n > capacity()` 时重新分配 |
| `shrink_to_fit` 退回多余容量 | \[vector.capacity\] | `noexcept`，可能创建新缓冲区搬移数据 |

### string 标准要求

| 要求 | 条款 | libc++ 实现方式 |
|---|---|---|
| 连续存储 | \[string.require\] | SSO 时 `__data_[23]` 连续；堆时 `__data_` 指针连续 |
| `operator[]` 返回引用 | \[string.access\] | SSO 和堆模式均返回直接引用，无 COW 延迟拷贝 |
| `data()` 返回可写指针 (C++17) | \[string.accessors\] | `__is_long()` 分支返回对应指针，可直接写入 |
| `c_str()` 与 `data()` 相同指针 | \[string.accessors\] | 两者实现相同，均调用 `__get_pointer()` |
| 非变异操作不使迭代器失效 | \[string.iterators\] | 无 COW，非变异操作不触发重新分配 |
| `reserve` 强异常保证 | \[string.capacity\] | 先分配新缓冲区再拷贝，失败则原 string 不变 |
| 移动赋值 noexcept | \[string.modifiers\] | `noexcept(POCMA \|\| is_always_equal)` |

## 核心源码路径

| 文件 | 职责 |
|---|---|
| `include/__vector/vector.h` | vector 主模板：三指针布局、`__recommend()`、`emplace_back`、`insert` 系列 |
| `include/__split_buffer` | `__split_buffer`：用于 vector 扩容的中间有洞缓冲区 |
| `include/__vector/vector_bool.h` | `vector<bool>` 特化：位压缩存储 |
| `include/string` | `basic_string` 主模板：SSO 布局、`__is_long()`、所有内联成员函数 |
| `include/__string/constexpr_c_functions.h` | constexpr 版本的 C 字符串操作（`strlen`、`memcpy` 等） |
| `include/__string/char_traits.h` | `char_traits<char>` 特化：`copy`、`move`、`compare` |
| `include/__memory/uninitialized_algorithms.h` | `__uninitialized_allocator_move_if_noexcept`：vector 搬移核心 |
| `include/__type_traits/is_trivially_relocatable.h` | `__libcpp_is_trivially_relocatable`：memcpy 优化的类型特征 |

## 核心类 / 函数

### vector 核心成员

```cpp
template <class _Tp, class _Allocator>
class vector {
  pointer __begin_;    // 数据起始
  pointer __end_;      // 有效元素末尾
  pointer __cap_;      // 分配容量末尾
  _Allocator __alloc_; // [[no_unique_address]] 压缩

  // 容量增长
  size_type __recommend(size_type __new_size) const;

  // 扩容核心
  void __swap_out_circular_buffer(_SplitBuffer& __v);

  // 搬移策略选择
  template <class _Up>
  _LIBCPP_CONSTEXPR_SINCE_CXX20 void __move_range(pointer __from_s, pointer __from_e, pointer __to);

  // 析构守卫
  class __destroy_vector;
};
```

### __split_buffer 核心成员

```cpp
template <class _Tp, class _Allocator, template<class,class,class> class _Layout>
class __split_buffer {
  pointer __front_cap_;  // 分配起始
  pointer __begin_;      // 有效元素起始
  pointer __end_;        // 有效元素末尾
  pointer __back_cap_;   // 分配末尾

  // front_spare = __begin_ - __front_cap_
  // back_spare  = __back_cap_ - __end_

  void __construct_at_end(size_type __n);
  void push_back(const_reference __x);
  void push_front(const_reference __x);
};
```

### string 核心成员

```cpp
// alternate layout（默认）
struct __long {
  pointer __data_;              // 堆指针
  size_type __size_;            // 字符串长度
  size_type __cap_ : 63;       // 容量（位域）
  size_type __is_long_ : 1;    // 长模式标志
};

struct __short {
  value_type __data_[23];       // 内联字符数据
  unsigned char __size_ : 7;   // 短字符串长度
  unsigned char __is_long_ : 1;// 长模式标志（=0）
};

// 关键成员函数
bool __is_long() const;         // 检查 __is_long_ 位
pointer __get_pointer();        // 返回 __is_long() ? __data_ : __data_[]
size_type __get_short_size() const;
size_type __get_long_size() const;
size_type __get_short_cap() const { return __min_cap - 1; }
size_type __get_long_cap() const;
```

## 关键算法

### vector 容量增长策略

```cpp
size_type __recommend(size_type __new_size) const {
  const size_type __ms = max_size();
  if (__new_size > __ms)
    this->__throw_length_error();
  const size_type __cap = capacity();
  if (__cap >= __ms / 2)
    return __ms;
  return std::max<size_type>(2 * __cap, __new_size);
}
```

增长因子为 **2×**：`max(2 * capacity, new_size)`。当 `capacity >= max_size / 2` 时直接返回 `max_size`，避免溢出。

与 libstdc++ 的差异：libstdc++ 使用 `~1.5×` 增长（`capacity + capacity / 2`），libc++ 使用 `2×`。

### split_buffer 重定位策略

```
原始 vector: [A B C D E]，insert(2, X)，容量不足

1. __recommend(6) = 10
2. 构造 split_buffer(10, 2)：
   ┌──────────────────────────────────────────┐
   │ ? ? ? ? ? ? ? ? ? ?                      │
   │         ↑__begin_=2（留空位置）            │
   └──────────────────────────────────────────┘
3. 先搬移后半段 [D, E] 到末尾：
   [? ? ? ? ? ? ? D E ?]
4. 再搬移前半段 [A, B] 到开头：
   [A B ? ? ? ? ? D E ?]
5. 在位置 2 构造 X：
   [A B X ? ? ? ? D E ?]
6. __swap_out_circular_buffer：交换指针，vector 现在指向新缓冲区
```

**为什么先搬后半段？** 异常安全：若搬移前半段时抛异常，后半段已在正确位置；若搬移后半段时抛异常，前半段仍在原位。

### string SSO 判断

```cpp
bool __is_long() const _NOEXCEPT {
  if (__libcpp_is_constant_evaluated() && __builtin_constant_p(__rep_.__l.__is_long_)) {
    return __rep_.__l.__is_long_;
  }
  return __rep_.__s.__is_long_;
}
```

SSO 判断在几乎所有 string 操作的热路径上执行。libc++ 通过位域优化：短模式最后字节的最低位 = 0，长模式 = 1。

### string 容量增长

```cpp
size_type __grow_by(size_type __old_cap,
                    size_type __delta_cap,
                    size_type __old_size,
                    size_type __n_copy,
                    size_type __n_del,
                    size_type __n_add) const;
```

增长策略：`max(2 * __old_cap, __old_cap + __delta_cap)`，与 vector 一致。

### trivially_relocatable memcpy 优化

```cpp
// vector.h 中的搬移路径选择
if constexpr (__libcpp_is_trivially_relocatable<value_type>::value &&
              is_trivially_move_constructible<value_type>::value &&
              is_trivially_destructible<value_type>::value &&
              !__libcpp_is_constant_evaluated()) {
  // __builtin_memcpy 一次搬移整个区间
} else {
  // 逐个 move_if_noexcept + destroy
}
```

## ABI 约束

### vector ABI 稳定性

- vector 的布局（三指针）在所有 libc++ 版本中保持稳定
- `__split_buffer` 是内部实现细节，不属于 ABI 承诺
- `__bounded_iter`（调试迭代器）通过 `_LIBCPP_ABI_BOUNDED_ITERATORS_IN_VECTOR` 宏启用

### string ABI 布局选项

| 宏 | 布局 | 说明 |
|---|---|---|
| `_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT` | **默认** | `__long` 末尾位域存 `__is_long_`，SSO 容量 = 22 |
| `_LIBCPP_ABI_STRING_PAIR_LAYOUT` | 可选 | `__long` 开头存 `__is_long_`，`__short` 数据在前，布局兼容旧 ABI |
| `_LIBCPP_ABI_NO_ITERATOR_BASES` | 可选 | 迭代器不继承 `__wrap_iter`，减小迭代器尺寸 |

### 大小端影响

```cpp
#ifdef _LIBCPP_BIG_ENDIAN
  static const size_type __endian_factor = 2;
#else
  static const size_type __endian_factor = 1;
#endif
```

大端系统下，`__cap_` 位域存储时需要乘以 `__endian_factor`，因为最高位（`__is_long_`）在大端下位于字节的另一端。

### 与 libstdc++/MSVC 的 ABI 兼容性

- libc++ string（24 字节）与 libstdc++ string（32 字节）**布局不兼容**
- 跨库传递 string 对象会导致未定义行为
- 安全做法：使用 `const char*` 或 `string_view` 跨 ABI 边界

## 异常安全

### vector 异常安全保证

| 操作 | 保证 | 机制 |
|---|---|---|
| `push_back(const T&)` | **强保证** | 若需扩容，先分配 split_buffer 再搬移元素；搬移失败则原数据不变 |
| `push_back(T&&)` | **强保证** | 同上；若 move 抛异常，split_buffer 析构时清理已构造元素 |
| `emplace_back(Args...)` | **强保证** | 同上；placement new 失败时 split_buffer 保证清理 |
| `insert(pos, n, val)` | **强保证** | split_buffer 中间留空，先搬移再构造，失败则原数据不变 |
| `erase(pos)` | **no-throw** | 逐个移动赋值 + 尾部析构，不涉及分配 |
| `reserve(n)` | **强保证** | 先分配新缓冲区，再搬移，再释放旧缓冲区 |
| `operator=(const vector&)` | **基本保证** | copy-and-swap 惯用法；拷贝失败时原 vector 可能已部分修改 |
| `operator=(vector&&)` | **no-throw** | 若 allocator 需传播则交换指针；否则逐个 move 赋值 |
| `clear()` | **no-throw** | 仅析构元素，不释放内存 |

### string 异常安全保证

| 操作 | 保证 | 机制 |
|---|---|---|
| `append(const char*, size_t)` | **强保证** | 若需扩容，先分配新缓冲区再拷贝；失败则原 string 不变 |
| `operator+=(const string&)` | **强保证** | 调用 `append`，继承其强保证 |
| `replace(pos, len, str)` | **强保证** | 若长度变化触发扩容，先分配新缓冲区；否则原地替换 |
| `insert(pos, str)` | **强保证** | 若需扩容，先分配新缓冲区再搬移 |
| `reserve(n)` | **强保证** | 先分配新缓冲区，再拷贝，再释放旧缓冲区 |
| `operator=(const string&)` | **基本保证** | 先释放旧缓冲区再拷贝；拷贝失败时 string 处于合法但未指定状态 |
| `operator=(string&&)` | **no-throw** | 交换指针和元数据，源 string 重置为空 SSO |
| `clear()` | **no-throw** | 仅设置长度为 0，不释放内存 |

### SSO 模式的异常安全优势

SSO 模式下（≤ 22 字节），string 操作不涉及堆分配，因此：
- 构造、赋值、append 等操作在 SSO 内为 no-throw
- 无 `bad_alloc` 风险，异常安全等级更高

## iterator / reference invalidation

### vector 失效规则

| 操作 | 迭代器/指针/引用是否失效 | 原因 |
|---|---|---|
| `push_back` 触发扩容 | **全部失效** | split_buffer 分配新缓冲区，旧缓冲区释放 |
| `push_back` 未触发扩容 | **不失效** | 仅在 `__end_` 处构造新元素 |
| `insert(pos, n, val)` | **全部失效**（若扩容）或 **pos 之后失效**（若未扩容） | 扩容时重新分配；否则 pos 之后的元素被搬移 |
| `erase(pos)` | **pos 之后失效** | pos 之后的元素被前移覆盖 |
| `erase(first, last)` | **first 之后全部失效** | 被删区间之后的元素被前移覆盖 |
| `reserve(n)` 若 `n > capacity()` | **全部失效** | 分配新缓冲区，释放旧缓冲区 |
| `reserve(n)` 若 `n ≤ capacity()` | **不失效** | 无操作 |
| `shrink_to_fit()` | **可能全部失效** | 可能创建更小的缓冲区搬移数据 |
| `clear()` | **不失效** | 仅析构元素，不释放内存 |
| `resize(n)` 若 `n > capacity()` | **全部失效** | 触发扩容 |
| `resize(n)` 若 `n ≤ capacity()` | **不失效** | 在现有缓冲区构造或析构元素 |
| `swap()` | **不失效** | 仅交换指针，元素地址不变 |

### string 失效规则

| 操作 | 迭代器/指针/引用是否失效 | 原因 |
|---|---|---|
| SSO → 堆转换（如 `append` 导致长度 > 22） | **全部失效** | `__get_pointer()` 从 `__data_[]` 切换到堆指针 |
| 堆内扩容（如 `append` 超出当前容量） | **全部失效** | 分配新堆缓冲区，释放旧缓冲区 |
| 堆内原地修改（长度不变） | **不失效** | 数据仍在同一堆缓冲区 |
| `shrink_to_fit()` | **可能全部失效** | 堆模式下可能重新分配更小缓冲区 |
| `clear()` | **不失效** | 仅设置长度为 0，不释放内存 |
| `reserve(n)` 若 `n > capacity()` | **全部失效** | 分配新缓冲区 |
| `swap()` | **全部失效** | 交换指针和元数据 |
| `erase()` | **不失效**（指向被删元素的引用失效） | 字符原地移动，长度缩短 |

### libc++ string 无 COW 失效陷阱

libc++ string 从 C++11 开始就使用 eager copy（深拷贝），不存在 COW 带来的失效陷阱：

```cpp
std::string a = "hello";
const char* p = a.data();
std::string b = a;           // 深拷贝，a 和 b 数据独立
assert(p == a.data());       // a 的指针仍有效——无 COW clone
```

libstdc++ 在 GCC 5 之前使用 COW，`operator[]` 的非 const 版本可能触发 `_M_unshare()`，导致已获取的迭代器失效。

## 性能模型

### vector 性能特征

| 参数 | libc++ | libstdc++ | MSVC | 影响 |
|---|---|---|---|---|
| 增长因子 | **2×** | ~1.5× | 1.5× | libc++ 分配次数更少但每次分配更大；libstdc++/MSVC 内存利用率更高 |
| 扩容搬移 | **memcpy**（trivially relocatable） | move + destroy | move + destroy | libc++ 对 trivial 类型使用一次 memcpy，更快 |
| `sizeof(vector)` | 24 字节 | 24 字节 | 24 字节 | 三指针布局相同 |
| SBO | 无 | 无 | 无 | vector 不使用小对象优化 |

### 增长因子权衡

```
2× 增长（libc++）：
  分配次数 = O(log₂ n)
  内存浪费 = 最多 ~50%（最后一次分配的未使用空间）
  分配调用 = 更少，每次分配更大

1.5× 增长（libstdc++/MSVC）：
  分配次数 = O(log₁.₅ n) ≈ 1.4× 更多分配
  内存浪费 = 最多 ~33%
  更好的内存回收复用（前几次分配的内存可能被后续分配复用）
```

### string SSO 性能特征

| 参数 | libc++ | libstdc++ | MSVC | 影响 |
|---|---|---|---|---|
| `sizeof(string)` | **24** | 32 | 32 | libc++ 对象更小，缓存友好 |
| SSO 容量 | **22** | 15 | 15 | libc++ 可内联更长的字符串 |
| SSO 构造 | memcpy 24 字节 | memcpy 32 字节 | memcpy 32 字节 | libc++ 更少的内存拷贝 |
| 堆构造 | malloc + memcpy | malloc + memcpy | malloc + memcpy | 三者相同 |
| 移动 SSO string | memcpy 24 字节 | memcpy 32 字节 | memcpy 32 字节 | SSO 无法偷指针，必须拷贝 |
| 移动堆 string | 交换指针 | 交换指针 | 交换指针 | O(1) |

### SSO 命中率分析

```
典型应用场景字符串长度分布（路径、邮件、标识符等）：
  ≤ 15 字节：~85%（libstdc++/MSVC SSO 命中）
  ≤ 22 字节：~92%（libc++ SSO 命中）

libc++ 额外覆盖 7% 的字符串 → 对短字符串密集场景（如 JSON 解析、日志处理）
性能提升显著
```

### 缓存行影响

```
sizeof(string) = 24 字节：
  一个 64 字节缓存行可容纳 2 个 string 对象（剩余 16 字节碎片）
  string 数组遍历时每 2 个对象触发一次缓存行加载

sizeof(string) = 32 字节（libstdc++/MSVC）：
  一个 64 字节缓存行可容纳 2 个 string 对象（无碎片）
  缓存利用率更高
```

## 编译 / benchmark 证据

### 验证 vector 三指针布局

```cpp
// vector_layout.cpp
#include <cstdio>
#include <vector>
#include <cstddef>

struct MyStruct {
  int a, b, c;
};

int main() {
  printf("sizeof(vector<int>) = %zu\n", sizeof(std::vector<int>));
  printf("sizeof(vector<MyStruct>) = %zu\n", sizeof(std::vector<MyStruct>));

  std::vector<int> v = {1, 2, 3, 4, 5};
  printf("data = %p\n", (void*)v.data());
  printf("size = %zu, capacity = %zu\n", v.size(), v.capacity());
}
```

编译运行：
```bash
clang++ -std=c++20 -O2 vector_layout.cpp -o vector_layout && ./vector_layout
# 预期输出（x86-64）：
# sizeof(vector<int>) = 24
# sizeof(vector<MyStruct>) = 24
# data = 0x...
# size = 5, capacity = 8
```

### 验证 SSO 边界（22 字节）

```cpp
// sso_boundary.cpp
#include <cstdio>
#include <string>
#include <cstring>

int main() {
  // 22 字节：SSO
  std::string short_str("0123456789012345678901");  // len = 22
  printf("short: data=%p obj=%p is_long=%d cap=%zu\n",
         (void*)short_str.data(), (void*)&short_str,
         short_str.size() > 22 ? 1 : 0,
         short_str.capacity());

  // 23 字节：堆分配
  std::string long_str("01234567890123456789012");   // len = 23
  printf("long:  data=%p obj=%p is_long=%d cap=%zu\n",
         (void*)long_str.data(), (void*)&long_str,
         long_str.size() > 22 ? 1 : 0,
         long_str.capacity());
}
```

编译运行：
```bash
clang++ -std=c++20 -O2 sso_boundary.cpp -o sso_boundary && ./sso_boundary
# 预期输出：
# short: data=0x7fff... obj=0x7fff... is_long=0 cap=22
# long:  data=0x555...   obj=0x7fff... is_long=1 cap=47
```

### vector 扩容 benchmark

```cpp
// vector_bench.cpp
#include <vector>
#include <benchmark/benchmark.h>

static void BM_PushBack(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int> v;
    for (int i = 0; i < state.range(0); ++i)
      v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
}

static void BM_ReservePushBack(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int> v;
    v.reserve(state.range(0));
    for (int i = 0; i < state.range(0); ++i)
      v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
}

BENCHMARK(BM_PushBack)->Range(8, 1 << 20);
BENCHMARK(BM_ReservePushBack)->Range(8, 1 << 20);
BENCHMARK_MAIN();
```

编译运行：
```bash
clang++ -std=c++20 -O2 -lbenchmark vector_bench.cpp -o vector_bench && ./vector_bench
# 预期结果（参考值，x86-64）：
# BM_PushBack/1024         ~800 ns
# BM_PushBack/1048576      ~1.2 ms
# BM_ReservePushBack/1024  ~300 ns  ← 无扩容开销
# BM_ReservePushBack/1048576 ~400 μs
```

### string SSO vs 堆 benchmark

```cpp
// string_bench.cpp
#include <string>
#include <benchmark/benchmark.h>

static void BM_StringSSO(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "hello world!";  // 12 字节，SSO
    benchmark::DoNotOptimize(s);
  }
}

static void BM_StringHeap(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "this is a long string that exceeds SSO threshold";  // 48 字节，堆
    benchmark::DoNotOptimize(s);
  }
}

static void BM_StringAppendSSO(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "hello";
    s += " world";  // 5+6=11，仍在 SSO 内
    benchmark::DoNotOptimize(s);
  }
}

static void BM_StringAppendHeap(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "hello world! this is a long string";
    s += " and more data";  // 触发堆分配
    benchmark::DoNotOptimize(s);
  }
}

BENCHMARK(BM_StringSSO);
BENCHMARK(BM_StringHeap);
BENCHMARK(BM_StringAppendSSO);
BENCHMARK(BM_StringAppendHeap);
BENCHMARK_MAIN();
```

典型 benchmark 结果（参考值，x86-64 Clang 17 -O2）：

| 场景 | 耗时（ns/op） | 说明 |
---|---|---|
| SSO 构造（12 字节） | ~5-8 | 纯 memcpy 24 字节 |
| 堆构造（48 字节） | ~15-25 | 含 malloc + memcpy |
| SSO append（11 字节） | ~5-8 | 原地追加，无分配 |
| 堆 append 触发扩容 | ~30-50 | 含新分配 + 拷贝 |

### trivially_relocatable memcpy 验证

```bash
# 查看 vector 扩容是否使用 memcpy
clang++ -std=c++20 -O2 -S -masm=intel vector_bench.cpp -o vector_bench.s
grep -A 20 'push_back' vector_bench.s | head -30
# 预期：对 trivial 类型（如 int），应看到 rep movsb 或 memcpy 调用而非循环
```

## cpplings 练习入口

- [`vector1` — vector 基础操作与扩容](../../../exercises/cpp11-std/vector1.cpp)
- [`stringview1` — std::string_view 非拥有字符串视图](../../../exercises/cpp17/stringview1.cpp)
- [`perf1` — 性能优化技巧：SBO、缓存友好、string_view](../../../exercises/topics/perf1.cpp)
