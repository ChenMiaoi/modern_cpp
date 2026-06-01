# libstdc++ (GCC) 源码级深度剖析

> libstdc++ 是 GCC 的 C++ 标准库实现，Linux 生态的事实标准。本文基于公开的 GCC 源码（libstdc++-v3），逐行分析核心组件的实现细节、对比 libc++/MSVC STL 的设计差异。

## 概述

libstdc++ 由 Per Bothner 于 1997 年启动，基于 SGI STL。25+ 年的演进使其成为 Linux 服务器、嵌入式、HPC 的标准库基础。核心设计理念：**ABI 稳定性优先于激进优化**。

---

## 1. std::string：从 COW 到 SSO 的 ABI 迁移

### 1.1 COW 时代（GCC 4.x，ABI v1）

libstdc++ 在 C++11 之前使用写时复制。**string 对象本身只有 1 个指针**（4 字节！）：

**COW string 内存布局（32 位系统，sizeof(basic_string) = 4 字节）**：

```
 string a            string b            string c
 ┌────────────┐      ┌────────────┐      ┌────────────┐
 │  _M_p  ●───┼──┐   │  _M_p  ●───┼──┐   │  _M_p  ●───┼──┐
 └────────────┘  │   └────────────┘  │   └────────────┘  │
                 │                   │                   │
                 ▼                   ▼                   │
  堆内存：  ┌───────────────────────────┐                 │
           │ _M_length    = 5          │                 │
           │ _M_capacity  = 15         │  共享块          │
           │ _M_refcount  = 3  ◄───────┼─────────────────┘
           ├───────────────────────────┤
           │ H  e  l  l  o  \0        │ ← _M_p 指向此处
           └───────────────────────────┘
            ▲                   ▲
            │                   │
      a._M_p ──────────────────┘ (同一地址)

 refcount 生命周期：
 ┌──────────────────────────────────────────────────────────┐
 │  string a("hello");            refcount = 1 (独占)        │
 │  string b = a;                 refcount = 2 (共享)        │
 │  string c = b;                 refcount = 3 (共享)        │
 │  a = "world";  // COW detach   refcount = 2, a 分配新块   │
 │  b.clear();    // 不触发 COW   refcount = 2               │
 │  c = "other";  // COW detach   refcount = 1               │
 │  // b 析构    refcount = 0 → _M_dispose() 释放            │
 └──────────────────────────────────────────────────────────┘

 全局空字符串实例（避免分配）：
 ┌────────────────────────┐
 │ _M_length   = 0        │
 │ _M_capacity = 0        │
 │ _M_refcount = -1       │ ← 特殊值：永不释放
 ├────────────────────────┤
 │ \0                     │
 └────────────────────────┘
 所有空 string 的 _M_p → 此全局实例
```

```cpp
class basic_string {
  _CharT* _M_p;  // 指向字符数据，前面紧跟 _Rep 头
};

// _Rep 头（在 _M_p 之前，通过指针算术访问）
struct _Rep {
  size_type    _M_length;     // 当前长度
  size_type    _M_capacity;   // 容量
  _Atomic_word _M_refcount;   // 原子引用计数
  // _M_refcount == -1 时表示空字符串（全局共享的空实例）
};
// 字符数据紧跟 _Rep 之后：[_Rep][chars...\0]
//                                         ^
//                                _M_p 指向这里
```

**引用计数的特殊值**：
- `_M_refcount == -1`：全局共享的空字符串（避免分配）
- `_M_refcount == 0`：独占（不共享）
- `_M_refcount > 0`：被多个 string 共享

**COW 的致命问题**（C++11 后不合规）：

```cpp
string a = "hello";
char& c = a[0];   // 获取引用
string b = a;      // COW：b 和 a 共享数据，refcount=2
c = 'H';           // 写入触发 lazy copy → b 被复制
                   // 但此时 c 引用的是旧数据还是新数据？
                   // 标准要求 c 始终引用 a[0]，COW 实现可能违反
```

### 1.2 SSO 迁移（GCC 5.0，ABI v2）

2015 年 GCC 5.0 引入新的非 COW string，通过 `__cxx11` 内联命名空间隔离 ABI：

**SSO string 内存布局（64 位系统，sizeof = 32 字节）**：

```
 ┌───────────────────────────────────────────────────────────────────────┐
 │                      basic_string 对象 (32 字节)                      │
 ├───────────────────────────────────────────────────────────────────────┤
 │ 偏移   0 ──  7  │ _M_dataplus._M_p       (8 字节，指向实际字符数据)    │
 │ 偏移   8 ── 15  │ _M_dataplus._M_alloc   (空 allocator，EBO 压缩为 0) │
 │ 偏移  16 ── 23  │ union: _M_string_length │ _M_local_buf[16] 前 8 字节 │
 │ 偏移  24 ── 31  │ _M_allocated_capacity  │ _M_local_buf[16] 后 8 字节 │
 └───────────────────────────────────────────────────────────────────────┘

 Short 模式（≤15 字节）：
 ┌───────────────────────────────────────────────────────────────────────┐
 │ _M_p ──────────────────────────────┐                                 │
 │                                    ▼                                 │
 │ [ _M_p=0x7fff0010 ][ local_buf: "hello\0          " ][  unused  ]    │
 │   ▲                   ▲                                              │
 │   │                   │                                              │
 │   └─ 指向自身         └─ _M_p == (char*)&_M_local_buf → SSO 判定     │
 └───────────────────────────────────────────────────────────────────────┘

 Long 模式（>15 字节）：
 ┌───────────────────────────────────────────────────────────────────────┐
 │ [ _M_p=0x55a00000 ][ _M_string_length=20 ][ _M_allocated_capacity ]  │
 │   ▲                                                                    │
 │   │   堆：                                                           │
 │   └── ┌──────────────────────────────────────┐                        │
 │       │ a  b  c  d  e  f  g  h  i  j  ... \0 │                        │
 │       └──────────────────────────────────────┘                        │
 │         _M_p != (char*)&_M_local_buf → 堆分配                         │
 └───────────────────────────────────────────────────────────────────────┘

 SSO 判定逻辑：
   _M_p == (char*)&_M_local_buf  →  Short (内联)
   _M_p != (char*)&_M_local_buf  →  Long  (堆)

 容量对比：
   libstdc++  SSO = 16 - 1 = 15 字节   sizeof = 32
   libc++     SSO = 23 - 1 = 22 字节   sizeof = 24
```

```cpp
// GCC 5.0+ basic_string 的实际布局
// sizeof = 32 字节
class basic_string {
  struct _Alloc_hider : _Allocator {
    _CharT* _M_p;  // 指向实际字符数据
  };
  _Alloc_hider _M_dataplus;  // 8 字节（指针）+ allocator（空时 0）

  union {
    size_type _M_string_length;  // Long 模式：字符串长度
    _CharT    _M_local_buf[16];  // Short 模式：15 字节内联 + \0
  };

  size_type _M_allocated_capacity;  // Long 模式：已分配容量
};

// SSO 判断：_M_p == (char*)&_M_local_buf  → SSO
//           _M_p != (char*)&_M_local_buf  → Heap
```

**SSO 容量**：16 字节 union - 1 字节 `\0` = 15 字节。比 libc++（22 字节）少 7 字节。

**为什么 libstdc++ 的 SSO 容量更小？** 因为 `sizeof` = 32（比 libc++ 的 24 多 8 字节）。多出的 8 字节来自 `_M_allocated_capacity`——libstdc++ 用独立字段存储容量，而 libc++ 将容量和标记位合并到同一个字段中。

### 1.3 双 ABI 共存

**ABI v1 → v2 符号分离机制**：

```
 ┌────────────────────────────────────────────────────────────────────┐
 │                     同一编译单元 / 同一 .so                        │
 │                                                                    │
 │  -D_GLIBCXX_USE_CXX11_ABI=0          默认 (=1)                   │
 │  ┌──────────────────────────────┐    ┌────────────────────────────┐│
 │  │ namespace std {              │    │ namespace std {            ││
 │  │   inline namespace __cxx11 { │    │   // (无内联命名空间)      ││
 │  │     class basic_string;      │    │   class basic_string;      ││
 │  │   }                          │    │ }                          ││
 │  │ }                            │    │                            ││
 │  └──────────────────────────────┘    └────────────────────────────┘│
 │                                                                    │
 │  编译后符号名：                                                     │
 │  ┌────────────────────────────────────────────────────────────────┐│
 │  │ ABI v1: _ZNSt12basic_stringIcSt11char_traitsIcESaIcEE...     ││
 │  │ ABI v2: _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESa... ││
 │  │                 ^^^^^^^                                        ││
 │  │                 命名空间注入 + [abi:cxx11] tag                  ││
 │  └────────────────────────────────────────────────────────────────┘│
 │                                                                    │
 │  两个不同类型的 string 可以在同一链接中共存！                        │
 └────────────────────────────────────────────────────────────────────┘

 迁移策略：
   Phase 1: -D_GLIBCXX_USE_CXX11_ABI=0  (全局旧 ABI)
   Phase 2: 逐模块改为 =1                 (混合 ABI，需要 .so 边界注意)
   Phase 3: 全部 =1                        (统一新 ABI，删除旧 ABI .so)
```

```cpp
// GCC 通过内联命名空间实现 ABI 版本化
inline namespace __cxx11 __attribute__((__abi_tag__("cxx11"))) {
  template<typename _CharT, typename _Traits, typename _Alloc>
  class basic_string;  // 新 ABI v2
}
// 旧 ABI v1 仍然存在，通过 -D_GLIBCXX_USE_CXX11_ABI=0 启用
```

**`abi_tag` 属性**：GCC 特有，将 ABI 标签编码到符号名中。`basic_string` 在新 ABI 中的符号名包含 `[abi:cxx11]`，与旧 ABI 的符号区分。这使得同一个 .so 中可以同时存在两种 ABI 的 string 类型。

---

## 2. std::vector：增长策略与异常安全

### 2.1 三指针布局

```cpp
// libstdc++ vector 的内部结构
struct _Vector_impl_data {
  pointer _M_start;           // 数据起始
  pointer _M_finish;          // size = _M_finish - _M_start
  pointer _M_end_of_storage;  // capacity = _M_end_of_storage - _M_start
};
```

**与 libc++ 完全相同的布局**——三个裸指针。`sizeof(vector<T>)` = 24（64 位，默认分配器）。

### 2.2 增长公式

```cpp
// libstdc++ vector::_M_check_len
size_type _M_check_len(size_type __n) const {
  if (max_size() - size() < __n)
    __throw_length_error("vector::_M_check_len");

  const size_type __len = size() + std::max(size(), __n);
  // 新容量 = size + max(size, 请求增量)
  // push_back 时：__len = size + size = 2*size（与 libc++ 相同）
  // insert(n个) 时：__len = size + max(size, n)
  return (__len < size() || __len > max_size()) ? max_size() : __len;
}
```

**对比 libc++**：libc++ 用 `max(2*cap, new_size)`，libstdc++ 用 `size + max(size, n)`。对 `push_back` 两者等价（2×）。对批量 `insert`，libstdc++ 可能增长更多（因为用 `size` 而非 `capacity` 作为基准）。

### 2.3 `__uninitialized_move_if_noexcept_a`

libstdc++ 的 vector reserve 没有 libc++ 的 trivially relocatable memcpy 优化。它使用传统的"move_if_noexcept + destroy"路径：

```cpp
// libstdc++ vector::reserve
void reserve(size_type __n) {
  if (__n > max_size()) __throw_length_error();
  if (__n > capacity()) {
    pointer __tmp = _M_allocate(__n);
    // 关键：如果 T 的 move 构造函数不是 noexcept，
    // 则 fallback 到 copy 构造（保证强异常安全）
    __uninitialized_move_if_noexcept_a(
        _M_impl._M_start, _M_impl._M_finish, __tmp, _M_get_Tp_allocator());
    std::_Destroy(_M_impl._M_start, _M_impl._M_finish, _M_get_Tp_allocator());
    _M_deallocate(_M_impl._M_start, capacity());
    _M_impl._M_start = __tmp;
    _M_impl._M_finish = __tmp + __old_size;
    _M_impl._M_end_of_storage = __tmp + __n;
  }
}
```

**`__uninitialized_move_if_noexcept_a` 的内部**：

```cpp
// 概念简化
template<class _Alloc, class _ForwardIterator, class _OutputIterator>
_OutputIterator __uninitialized_move_if_noexcept_a(
    _ForwardIterator __first, _ForwardIterator __last,
    _OutputIterator __result, _Alloc& __alloc) {
  // 使用 std::__make_move_if_noexcept_iterator 包装迭代器
  // 如果 T 是 nothrow move constructible → 返回 move_iterator
  // 否则 → 返回原迭代器（走 copy 路径）
  return std::__uninitialized_copy_a(
      std::__make_move_if_noexcept_iterator(__first),
      std::__make_move_if_noexcept_iterator(__last),
      __result, __alloc);
}
```

**与 libc++ 的关键差异**：libstdc++ 始终走 move/copy + destroy 路径。libc++ 的 `__uninitialized_allocator_relocate` 对 trivially relocatable 类型直接 memcpy——这在 vector 元素是 `unique_ptr`、`shared_ptr` 等简单类型时快得多。

### 2.4 insert 中间插入

libstdc++ 的 `insert` 没有 split_buffer 概念。中间插入时：

```cpp
// libstdc++ vector::insert(position, value) 简化
iterator insert(const_iterator __position, const value_type& __x) {
  pointer __p = _M_erase(const_cast<pointer>(__position));
  if (this->_M_finish != this->_M_end_of_storage) {
    // 有空间：移动后半段，放置新元素
    _Alloc_traits::construct(this->_M_impl, this->_M_finish, *(this->_M_finish - 1));
    std::copy_backward(__p, this->_M_finish - 1, this->_M_finish);
    *__p = __x;
    ++this->_M_finish;
  } else {
    // 无空间：先分配新缓冲区，再分段复制
    const size_type __len = _M_check_len(1);
    pointer __old_start = this->_M_impl._M_start;
    pointer __new_start = _M_allocate(__len);
    // 前半段 move
    pointer __new_finish = std::__uninitialized_move_if_noexcept_a(
        __old_start, __p, __new_start, _M_get_Tp_allocator());
    // 新元素
    _Alloc_traits::construct(this->_M_impl, __new_finish, __x);
    ++__new_finish;
    // 后半段 move
    __new_finish = std::__uninitialized_move_if_noexcept_a(
        __p, this->_M_finish, __new_finish, _M_get_Tp_allocator());
    // 清理旧缓冲区
    std::_Destroy(__old_start, this->_M_finish, _M_get_Tp_allocator());
    _M_deallocate(__old_start, capacity());
    // 更新指针
    this->_M_impl._M_start = __new_start;
    this->_M_impl._M_finish = __new_finish;
    this->_M_impl._M_end_of_storage = __new_start + __len;
  }
}
```

**与 libc++ 的对比**：libc++ 的 split_buffer 方案将旧元素一次性 relocate 到"带洞"的新缓冲区中，避免了"先 move 前半段，构造新元素，再 move 后半段"的三步操作。

---

## 3. SwissTable 集成：GCC 11+ 的 `unordered_map`

GCC 11 起，libstdc++ 的 `unordered_map`/`set` 底层从链式哈希表切换到 SwissTable（开放寻址 + SIMD 探测）：

**SwissTable 内存布局与 SSE2 探测**：

```
 libstdc++ SwissTable 交错布局（ctrl 字节与 slot 交错排列）：
 ┌───────────────────────────────────────────────────────────────────────┐
 │  group_size = 16（一个 SSE2 寄存器宽度）                              │
 │                                                                       │
 │  偏移:   [0]  [1]  [2]  [3]  ... [15]  │  [16] [17] [18] ...         │
 │         ┌────┬────┬────┬────┬────┬────┐│┌────┬────┬────┬────┬───     │
 │  ctrl:  │ H2 │ 80 │ H2 │ FE │ H2 │ FF │││ H2 │ 80 │ H2 │ H2 │ ...  │
 │         │占位 │空  │占位 │删除 │占位 │哨兵│││占位 │空  │占位 │占位 │       │
 │         ├────┼────┼────┼────┼────┼────┤│├────┼────┼────┼────┼───     │
 │  slot:  │ K  │    │ K  │    │ K  │    │││ K  │    │ K  │ K  │ ...   │
 │         │ V  │    │ V  │    │ V  │    │││ V  │    │ V  │ V  │       │
 │         └────┴────┴────┴────┴────┴────┘│└────┴────┴────┴────┴───     │
 │                                                                       │
 │  控制字节编码：                                                       │
 │  ┌───────────────────────────────────────────────────────────┐        │
 │  │  0b0xxxxxxx  → 已占用，低 7 位 = H2 哈希值 (0x00~0x7F)   │        │
 │  │  0b10000000  → kEmpty    (-128)                           │        │
 │  │  0b11111110  → kDeleted  (-2)                             │        │
 │  │  0b11111111  → kSentinel (-1)     ← 永驻末尾，探测终止     │        │
 │  └───────────────────────────────────────────────────────────┘        │
 │                                                                       │
 │  SSE2 探测过程（查找 key，H1=桶索引，H2=低 7 位）：                   │
 │                                                                       │
 │  Step 1: 加载 16 个 ctrl 字节到 __m128i                              │
 │    ctrl = _mm_load_si128(ctrl_ + pos)                                 │
 │                                                                       │
 │  Step 2: 广播 H2 哈希值到 16 字节                                     │
 │    match = _mm_set1_epi8(h2)    → [h2, h2, h2, h2, ..., h2]         │
 │                                                                       │
 │  Step 3: 逐字节比较，取 movemask                                       │
 │    mask = _mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, match))              │
 │    例: ctrl = [0x3A, 0x80, 0x3A, 0xFE, ...]                          │
 │         h2   = 0x3A                                                   │
 │    cmp  = [0xFF, 0x00, 0xFF, 0x00, ...]                              │
 │    mask = 0b0000000000000101 = 0x0005                                  │
 │    → bit 0 和 bit 2 为候选匹配                                        │
 │                                                                       │
 │  Step 4: 对候选 slot 执行完整 key 比较（H1 + key == key）             │
 │                                                                       │
 │  缓存行优势：ctrl[i] 与 slot[i] 相邻 → 探测到 ctrl 命中时，           │
 │  对应 slot 大概率已在同一缓存行内，无额外 cache miss                   │
 └───────────────────────────────────────────────────────────────────────┘
```

```cpp
// libstdc++ SwissTable 的控制字节
// 每个 slot 对应 1 个控制字节
enum Ctrl : int8_t {
  kEmpty    = -128,   // 0b10000000
  kDeleted  = -2,     // 0b11111110
  kSentinel = -1,     // 0b11111111
};
// 已占用：0b0xxxxxxx，低 7 位 = H2 哈希值

// SSE2 探测：一次比较 16 个控制字节
__m128i ctrl = _mm_load_si128((__m128i*)(ctrl_ + pos));
__m128i match = _mm_set1_epi8(h2_hash);
uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, match));
// mask 的每个置位位对应一个可能的匹配 slot
```

**与 Abseil 的差异**：

| 维度 | libstdc++ SwissTable | Abseil SwissTable |
|------|---------------------|-------------------|
| 控制字节和 slots | 交错（ctrl[i] 对应 slot[i]） | 分离（ctrl 数组 + slot 数组） |
| 增长策略 | 7/8 负载因子 | 7/8 负载因子 |
| 异构查找 | 支持（透明 hash/eq） | 支持 |
| 插值探测 | 线性探测（+1, +2, +3...） | 线性探测 |

libstdc++ 的交错布局意味着 ctrl 和对应的 slot 在同一缓存行中——探测命中后访问 slot 不需要额外的 cache miss。

---

## 4. std::shared_ptr 的控制块设计

### 4.1 `_Sp_counted_base`：引用计数基类

```cpp
// libstdc++ shared_ptr 的控制块
template<typename _Lp>
class _Sp_counted_base {
  _Atomic_word  _M_use_count;    // 强引用计数（atomic）
  _Atomic_word  _M_weak_count;   // 弱引用计数（atomic）
  // 弱引用计数 = 实际 weak_ptr 数 + (use_count > 0 ? 1 : 0)

  virtual void _M_dispose() = 0;      // use_count → 0
  virtual void _M_destroy() = 0;      // weak_count → 0
  virtual void* _M_get_deleter() = 0; // type-erased deleter
};
```

### 4.2 `make_shared` 的 `_Sp_counted_ptr_inplace`

**`make_shared` 单次分配布局**：

```
 make_shared<T>(args...) 只调用一次 malloc：

 shared_ptr a           shared_ptr b           weak_ptr w
 ┌──────────────┐       ┌──────────────┐       ┌──────────────┐
 │ _M_ptr  ●────┼──┐    │ _M_ptr  ●────┼──┐    │ _M_ptr  ●────┼──┐
 │ _M_cntrl ●───┼─┐│    │ _M_cntrl ●───┼─┐│    │ _M_cntrl ●───┼─┐│
 └──────────────┘ ││    └──────────────┘ ││    └──────────────┘ ││
                  ││                     ││                     ││
   单次 malloc：  ││                     ││                     ││
   ┌──────────────┼┼─────────────────────┼┼─────────────────────┘│
   │              ▼▼                     ▼▼                      │
   │  _Sp_counted_ptr_inplace<T, Alloc>                         │   │
   │  ┌──────────────────────────────────────────────────────┐   │
   │  │ +0:  vptr                  (8B)  → 虚函数表           │   │
   │  │ +8:  _M_use_count  = 2    (4B)  ← a 和 b            │   │
   │  │ +12: _M_weak_count = 1    (4B)  ← w + 隐式 +1       │   │
   │  │ +16: _M_impl._M_alloc     (0B)  ← 空 allocator EBO  │   │
   │  │ +16: _M_storage           (sizeof(T), alignof(T))    │   │
   │  │      ┌──────────────────────────────────────────┐    │   │
   │  │      │  T 对象（原地构造）                        │    │   │
   │  │      │  通过 _M_ptr() = reinterpret_cast<T*>()  │    │   │
   │  │      │  访问                                    │    │   │
   │  │      └──────────────────────────────────────────┘    │   │
   │  └──────────────────────────────────────────────────────┘   │
   └────────────────────────────────────────────────────────────┘

 与 new shared_ptr<T>(new T) 的对比：
   make_shared: 1 次 malloc (控制块 + 对象 连续)
   new 方式:    2 次 malloc (控制块 + 对象 分离)

 析构序列：
   use_count → 0:  _M_dispose()  → T::~T()  (仅析构，不释放)
   weak_count → 0: _M_destroy()  → ~_Sp_counted_ptr_inplace()
                                 → allocator.deallocate(this, 1)  (释放整块)
```

```cpp
template<typename _Tp, typename _Alloc>
class _Sp_counted_ptr_inplace : public _Sp_counted_base {
  struct _Impl : _Sp_ebo_helper<0, _Alloc> {
    // 通过空基类优化压缩 allocator
  };
  _Impl _M_impl;

  // 元素存储在控制块末尾
  // 通过 aligned_storage 或实际类型存储
  typename aligned_storage<sizeof(_Tp), alignof(_Tp)>::type _M_storage;

  _Tp* _M_ptr() { return reinterpret_cast<_Tp*>(&_M_storage); }

  void _M_dispose() override {
    _M_ptr()->~_Tp();  // 只析构，不释放内存
  }
  void _M_destroy() override {
    // 释放控制块 + 元素的整块内存
    _Alloc __a(_M_impl._M_alloc());
    this->~_Sp_counted_ptr_inplace();
    __a.deallocate(static_cast<void*>(this), 1);
  }
};
```

**与 libc++ 的差异**：libc++ 使用 `_LIBCPP_COMPRESSED_PAIR` 宏 + `reinterpret_cast` 的 `_Storage` 内部类。libstdc++ 使用 `_Sp_ebo_helper`（空基类优化辅助模板）+ `aligned_storage`。两者最终效果相同：对象和控制块在一次 `malloc` 中。

---

## 5. `_Rb_tree`：红黑树的节点复用

### 5.1 节点结构

```cpp
struct _Rb_tree_node_base {
  typedef _Rb_tree_node_base* _Base_ptr;
  _Rb_tree_color  _M_color;    // 枚举：_S_red = false, _S_black = true
  _Base_ptr       _M_parent;
  _Base_ptr       _M_left;
  _Base_ptr       _M_right;

  // 静态辅助函数（非虚，零开销）
  static _Base_ptr _S_minimum(_Base_ptr __x) {
    while (__x->_M_left) __x = __x->_M_left;
    return __x;
  }
  static _Base_ptr _S_maximum(_Base_ptr __x) {
    while (__x->_M_right) __x = __x->_M_right;
    return __x;
  }
};

template<typename _Tp>
struct _Rb_tree_node : _Rb_tree_node_base {
  _Tp _M_value_type;  // 数据存在节点末尾
  // 通过 __addressof 取值地址，避免 operator& 重载
};
```

### 5.2 容器头：`_M_impl`

**`_M_header` 哨兵节点与红黑树的连接关系**：

```
 _M_impl._M_header（哨兵节点，始终标红）
 ┌──────────────────────────────────────────────────────────────────────────┐
 │                                                                          │
 │  空树时（_M_reset 后）：                                                  │
 │                                                                          │
 │              _M_header                                                    │
 │             ┌────────────┐                                               │
 │             │ color = red│                                               │
 │    _M_left──┤ ●──────┐   │                                               │
 │             │ ●──────┼─┐ │  ← _M_parent = nullptr（无根）                │
 │    _M_right─┤ ●──────┼─┼─┤                                               │
 │             └────────┘ │ │                                               │
 │               ▲        │ │                                               │
 │               └────────┘ │  ← _M_left 指向自身                           │
 │                 ▲        │                                               │
 │                 └────────┘  ← _M_right 指向自身                          │
 │                                                                          │
 │  非空树（含 {1, 3, 5, 7, 9}）：                                          │
 │                                                                          │
 │                         _M_header                                        │
 │                        ┌──────────┐                                      │
 │                        │ color=red│                                      │
 │         _M_left(min)◄──┤ ●────────┤──► _M_right(max)                    │
 │                        │ ●──────┐ │     = node 9                        │
 │                        │ ●────┐ │ │                                      │
 │                        └──────┼─┼─┘                                      │
 │                    ▲         │ │                                         │
 │                    │         │ └──► _M_parent = root = node 5            │
 │                    │         ▼                                            │
 │               ┌────┼── node 5 (B) ──────┐                               │
 │               │    │   _M_parent → hdr   │                               │
 │               │    ▼                      ▼                               │
 │           node 3 (R)                 node 7 (B)                          │
 │           ┌────┘    └────┐           ┌────┘    └────┐                   │
 │       node 1 (B)    node ?   node ?         node 9 (R)                  │
 │                                                                         │
 │  关键指针语义：                                                          │
 │   header._M_parent  = 根节点 (node 5)                                    │
 │   header._M_left    = 最左节点 (node 1) → begin()                        │
 │   header._M_right   = 最右节点 (node 9) → --end() O(1)                  │
 │   root._M_parent    = header → 用于 end() 判定                           │
 │   node 9._M_right   = nullptr（真正的 nullptr）                           │
 │                                                                         │
 │  迭代器递增：                                                            │
 │   ++it: 若右子树存在 → 右子树最左; 否则沿 parent 上溯直到从左上来        │
 │   end(): it._M_node == &header                                          │
 └──────────────────────────────────────────────────────────────────────────┘
```

```cpp
template<typename _Key, typename _Val, typename _KeyOfValue, typename _Compare, typename _Alloc>
class _Rb_tree {
  struct _Rb_tree_impl : public _Node_allocator {
    _Rb_tree_key_compare  _M_key_compare;  // 比较器（空时通过 EBO 零开销）
    _Rb_tree_node_base    _M_header;        // 哨兵节点（相当于 libc++ 的 __end_node_）
    size_type             _M_node_count;    // 节点计数

    _Rb_tree_impl() : _Node_allocator(), _M_key_compare(),
                      _M_header(), _M_node_count(0) {
      _M_header._M_color = _S_red;  // 哨兵标红（便于区分）
      _M_reset();
    }
  };
  _Rb_tree_impl _M_impl;

  void _M_reset() {
    _M_impl._M_header._M_parent = nullptr;  // 根节点
    _M_impl._M_header._M_left = &_M_impl._M_header;  // begin() = header（空树时）
    _M_impl._M_header._M_right = &_M_impl._M_header; // end() = header
    _M_impl._M_node_count = 0;
  }
};
```

**`_M_header` 哨兵节点**：与 libc++ 的 `__end_node_` 相同的作用。`_M_header._M_left` 指向最左节点（begin），`_M_header._M_parent` 指向根节点。`_M_header._M_right` 指向最右节点（`--end()` 可 O(1)）。

### 5.3 插入时的 hint 优化

libstdc++ 的 `insert_unique` 支持 hint 迭代器——告诉树"新元素大概在哪个位置附近"：

```cpp
// 如果 hint 指向的节点是新元素的直接前驱或后继，插入是 O(1)
iterator insert_unique(iterator __position, const value_type& __v) {
  // 检查 __position 是否是有效的插入 hint
  if (__position._M_node == _M_impl._M_header._M_left) {
    // hint = begin()：如果新元素 < 最小元素，直接插到左边
    if (size() > 0 && _M_key_compare(_KeyOfValue()(__v), _S_key(__position._M_node)))
      return _M_insert_(__position._M_node, __position._M_node, __v);
  } else if (__position._M_node == &_M_impl._M_header) {
    // hint = end()：如果新元素 > 最大元素，直接插到右边
    if (_M_key_compare(_S_key(_M_impl._M_header._M_right), _KeyOfValue()(__v)))
      return _M_insert_(nullptr, _M_impl._M_header._M_right, __v);
  }
  // 一般情况：fallback 到标准 O(log n) 插入
  return insert_unique(__v).first;
}
```

---

## 6. std::function 的 SBO

libstdc++ 的 `std::function` 使用与 libc++ 相同大小的 SBO 缓冲区（24 字节），但内部管理方式不同：

```cpp
template<typename _Res, typename... _ArgTypes>
class function<_Res(_ArgTypes...)> {
  // 24 字节对齐存储
  typedef typename aligned_storage<3 * sizeof(void*)>::type _Any_data;

  // 虚函数表的函数指针
  typedef _Res (*_Invoker_type)(const _Any_data&, _ArgTypes&&...);

  _Any_data      _M_functor;   // callable 存储（栈或堆）
  _Invoker_type  _M_invoker;   // 调用分发函数指针
  // 管理器函数指针（用于拷贝/析构/swap）
  void (*_M_manager)(_Any_data&, const _Any_data&, _Manager_operation);
};
```

**与 libc++ 的差异**：
- libc++ 使用虚函数（`__base` 基类 + `__func` 派生类）实现多态
- libstdc++ 使用**函数指针**（`_M_invoker` 和 `_M_manager`）代替虚函数

函数指针方案的优势：避免了虚函数表的间接调用开销（一次直接函数调用 vs 一次 vtable 加载 + 间接调用）。在频繁调用 `std::function` 的热路径上，这可能有微小的性能差异。

---

## 7. ABI 稳定性：`_GLIBCXX_USE_CXX11_ABI` 宏

libstdc++ 的 ABI 管理策略比 libc++ 更保守：

```cpp
// 编译器自动定义的宏
#if _GLIBCXX_USE_CXX11_ABI
// 新 ABI v2：SSO string, 空 std::list, std::string_view, ...
inline namespace __cxx11 {
  template<typename _CharT, typename _Traits, typename _Alloc>
  class basic_string;
}
#else
// 旧 ABI v1：COW string, 旧 list 实现
// 通过 -D_GLIBCXX_USE_CXX11_ABI=0 显式启用
#endif
```

**ABI 稳定的代价**：libstdc++ 不得不保留一些次优实现。例如 `std::list` 的 `size()` 必须是 O(1)（ABI 锁定），即使这使得 `splice` 操作需要 O(n) 的计数更新。

---

## 8. PMR 与分配器

### 8.1 `std::allocator` 的空基类优化

```cpp
template<typename _Tp>
class allocator {
  // libstdc++ 的 std::allocator 是完全无状态的
  // allocate 调用 ::operator new
  // deallocate 调用 ::operator delete
  // sizeof(allocator<T>) == 1（空类）
};
```

libstdc++ 的 `_Vector_base` 使用 `_Vector_impl`（继承 `_Vector_impl_data` + `_Tp_alloc_type`），当 allocator 为空时通过空基类优化不占空间。

### 8.2 `std::pmr` 实现

与 libc++ 一致：`memory_resource` 虚基类，`polymorphic_allocator` 持有指针。libstdc++ 的 `pmr` 实现没有特别的优化技巧——标准的虚函数分发。

---

## 三实现综合对比

| 组件 | libstdc++ (GCC) | libc++ (LLVM) | MSVC STL |
|------|----------------|--------------|----------|
| sizeof(string) | 32 | 24 | 32 |
| string SSO | 15 | 22 | 15 |
| string COW | 有（ABI v1） | 无 | 无 |
| vector 增长 | size + max(size,n) | 2×cap | 1.5×cap |
| vector relocate | move+destroy | memcpy (trivial) | move+destroy |
| vector insert | 三段复制 | split_buffer | 三段复制 |
| unique_ptr | 8 B | 8 B | 8 B |
| trivial_abi unique_ptr | 不支持 | 支持 | 不支持 |
| shared_ptr make_shared | 1 次 malloc | 1 次 | 1 次 |
| function 多态 | 函数指针 | 虚函数 | 函数指针 |
| function SBO | 24 B | 24 B | 不同 |
| map/set | _Rb_tree | __tree | RB-tree |
| unordered_map | SwissTable (GCC 11+) | 链式 | 链式 |
| tuple | 递归继承 | 递归继承 | flat |
| Ranges | 渐进补齐 | 最早完整 | 较晚 |
| format | GCC 15 完整 | LLVM 20 | MSVC 19.38+ |
| ABI 稳定性 | 最强（abi_tag） | 版本化 | 最强 |
| Trivially relocatable | 无 | 探索中 | 无 |

---

## 选择建议

**选 libstdc++**：Linux 服务器、最强 ABI 兼容、GCC 用户、SwissTable 性能、HPC。

**选 libc++**：macOS/iOS/Android、最紧凑布局（string 24 B, SSO 22）、trivially relocatable memcpy、最新 C++ 标准。

**选 MSVC STL**：Windows、最好 `std::format` 浮点性能、flat tuple 编译更快。

**跨平台通用原则**：始终标记 move 构造为 `noexcept`（libstdc++ 的 `__uninitialized_move_if_noexcept` 依赖此）。避免依赖 `sizeof(string)` 或 `sizeof(vector)`。避免假设 `unordered_map` 的迭代顺序。
