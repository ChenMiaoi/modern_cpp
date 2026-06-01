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
