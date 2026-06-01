# libstdc++ vector 与 SwissTable

## std::vector

### 三指针布局

```cpp
struct _Vector_impl_data {
  pointer _M_start;           // 数据起始
  pointer _M_finish;          // size = _M_finish - _M_start
  pointer _M_end_of_storage;  // capacity = _M_end_of_storage - _M_start
};
// sizeof(vector<T>) = 24（与 libc++ 完全相同）
```

### 增长公式

```cpp
size_type _M_check_len(size_type __n) const {
  const size_type __len = size() + std::max(size(), __n);
  // push_back: 新容量 = size + size = 2×size（与 libc++ 相同）
  // insert(n个): 新容量 = size + max(size, n)
  return (__len < size() || __len > max_size()) ? max_size() : __len;
}
```

**与 libc++ 的差异**：libstdc++ 始终走 move_if_noexcept + destroy 路径，没有 libc++ 的 trivially relocatable memcpy 优化。

### 中间插入：三段复制

```cpp
// 无 split_buffer 概念，直接三段操作：
// 1. move 后半段到新位置
// 2. 构造新元素
// 3. move 前半段（如果有扩容）
```

## SwissTable 集成（GCC 11+）

GCC 11 起，`unordered_map`/`set` 底层从链式哈希表切换到 SwissTable。

### 交错布局

```
 libstdc++ SwissTable（ctrl 与 slot 交错）:
 ┌────────────────────────────────────────────────────┐
 │  偏移:  [0]  [1]  [2]  [3]  ... [15]  │  [16] ... │
 │  ctrl:  │ H2 │ 80 │ H2 │ FE │ H2 │ FF │  │ H2 │ 80 │
 │  slot:  │ K  │    │ K  │    │ K  │    │  │ K  │    │
 │         │ V  │    │ V  │    │ V  │    │  │ V  │    │
 └────────────────────────────────────────────────────┘

 控制字节编码：
   0b0xxxxxxx → 已占用，低 7 位 = H2
   0b10000000 → kEmpty (-128)
   0b11111110 → kDeleted (-2)
   0b11111111 → kSentinel (-1)

 SSE2 探测：
   ctrl = _mm_load_si128((__m128i*)(ctrl_ + pos))
   match = _mm_set1_epi8(h2_hash)
   mask = _mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, match))
```

**交错布局的优势**：ctrl[i] 和 slot[i] 在同一缓存行中——探测命中后访问 slot 不需要额外 cache miss。

**与 Abseil 的差异**：Abseil 使用分离布局（ctrl 数组 + slot 数组），16 个 ctrl 字节正好一个 cache line，探测效率更高但命中后可能需要额外 cache miss。
