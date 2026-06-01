# Folly (Meta) 源码级深度剖析

> 本文基于 Folly 源码（`references/impl/folly/folly/`），逐行分析 FBString 三级存储、F14 哈希表、IOBuf 零拷贝缓冲区、Future/Promise 异步框架、Synchronized 线程安全包装的实现细节。

## 1. fbstring：三级存储的字节级实现

### 1.1 存储布局
### 字节级布局图

```
fbstring 24 字节三种模式布局（小端序 64 位系统）:

┌─────────────────────────────── Small（≤ 23 字节）──────────────────────────┐
│                                                                            │
│   字节:  0  1  2  3  ...  20  21  22 │ 23                                │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┐                                │
│  │ c0 │ c1 │ c2 │ c3 │    │ c20│ c21│ sz │                                │
│  └────┴────┴────┴────┴────┴────┴────┴────┘                                │
│   ├──── 字符数据（最多 23 字节）────┤│(23-size)<<2│                        │
│                                                                            │
│   字节 23 的位域：                                                         │
│   ┌──┬──────────────────────────┐                                         │
│   │00│  23 - size（右移 2 位）   │  ← 低 2 位 = 00 标记 Small             │
│   └──┴──────────────────────────┘                                         │
│   巧妙：size==23 时 byte[23]==0，自然形成 null 终止符                      │
└────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────── Medium（24~254 字节）───────────────────────┐
│                                                                            │
│   字节:  0 ─────── 7   8 ──────── 15  16 ──────── 23                      │
│  ┌──────────────┬────────────────┬────────────────┐                        │
│  │  data_ (ptr) │  size_         │  capacity_     │                        │
│  │  Char* 8B    │  size_t 8B     │  size_t 8B     │                        │
│  └──────────────┴────────────────┴────────────────┘                        │
│                                  │                   │                     │
│                                  └─ bit[63] = 1 ────┘ 标记 Medium         │
│                                                                            │
│  data_ ──→ ┌────┬────┬────┬────┬────┬───┐                                 │
│            │ c0 │ c1 │ c2 │    │ cn │ \0│  堆上 malloc 分配                │
│            └────┴────┴────┴────┴────┴───┘                                  │
│              eager copy：复制时 malloc 新缓冲区，完整拷贝数据               │
└────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────── Large（≥ 255 字节，COW）────────────────────┐
│                                                                            │
│   字节:  0 ─────── 7   8 ──────── 15  16 ──────── 23                      │
│  ┌──────────────┬────────────────┬────────────────┐                        │
│  │  data_ (ptr) │  size_         │  capacity_     │                        │
│  │  Char* 8B    │  size_t 8B     │  size_t 8B     │                        │
│  └──────┬───────┴────────────────┴────────────────┘                        │
│         │     bit[62] = 1 标记 Large                                       │
│         └──→ data_ 指向 RefCounted::data_[0]（见下方）                     │
│                                                                            │
│   最后字节 bit[63:62] 编码（categoryExtractMask = 0xC0）：                  │
│   ┌──┬──┬──────────────────────────────┐                                   │
│   │00│  │      (23 - size) << shift    │  → Small                          │
│   ├──┤  ├──────────────────────────────┤                                   │
│   │1X│  │      capacity 高位            │  → Medium (bit63=1)              │
│   ├──┤  ├──────────────────────────────┤                                   │
│   │X1│  │      capacity 高位            │  → Large  (bit62=1)              │
│   └──┴──┴──────────────────────────────┘                                   │
└────────────────────────────────────────────────────────────────────────────┘

┌──── RefCounted 内存布局（Large 模式）──────────────────────────────────────┐
│                                                                            │
│  data_ 指针 ──→ RefCounted::data_[0]                                       │
│                 ↓                                                          │
│  堆内存：                                                                  │
│  偏移:  -8 (或 -N)    0   1   2       N-1                                 │
│        ┌──────────┬───┬───┬───┬──────┬────┐                                │
│        │ refCount │ d0│ d1│ d2│ ...  │ \0 │                                │
│        │ atomic   │   │   │   │      │    │                                │
│        │ size_t   │   │   │   │      │    │                                │
│        └──────────┴───┴───┴───┴──────┴────┘                                │
│        ↑                ↑                                                  │
│   getDataOffset()    data_[0]                                              │
│   返回此偏移                                                          │
│                                                                            │
│  fromData(p) = (char*)p - getDataOffset()  → 得到 RefCounted* 头指针       │
│  多个 Large fbstring 的 data_ 指向同一块 → 共享 refCount                    │
│  decrementRefs: fetch_sub(1) == 1 时 free 整块内存                         │
└────────────────────────────────────────────────────────────────────────────┘
```
fbstring 在 64 位系统上占 24 字节（与 libc++ string 相同），但使用**三级**而非两级存储：

```
24 字节 union 布局：

Small（≤ 23 字节）:
  字节 [0..22]: 字符数据
  字节 [23]:    (23 - size) << shift  （最后两位 = 00 标记 Small）

Medium（24~254 字节）:
  字节 [0..7]:   Char* data_         （堆分配指针）
  字节 [8..15]:  size_t size_
  字节 [16..23]: size_t capacity_    （最高位 = 1 标记 Medium）

Large（≥ 255 字节，COW）:
  布局与 Medium 相同，但 capacity_ 次高位 = 1 标记 Large
  data_ 前面有 RefCounted 头（atomic refcount + data）
```

### 1.2 类别编码（源码）

```cpp
// 源码：FBString.h:561
enum class Category : uint8_t {
  isSmall  = 0,                        // 最后字节两位都不设
  isMedium = kIsLittleEndian ? 0x80 : 0x2,  // LE: 最高位
  isLarge  = kIsLittleEndian ? 0x40 : 0x1,  // LE: 次高位
};

Category category() const {
  return static_cast<Category>(bytes_[lastChar] & categoryExtractMask);
  // lastChar = sizeof(MediumLarge) - 1 = 23
  // categoryExtractMask = 0xC0 (LE) → 提取最后字节的高 2 位
}
```

### 1.3 Small 模式：SSO 容量 23 字节

```cpp
// 源码：FBString.h:595
constexpr static size_t lastChar = sizeof(MediumLarge) - 1;  // = 23
constexpr static size_t maxSmallSize = lastChar / sizeof(Char);  // = 23 (for char)

// Small 模式的 size 编码：
// small_[23] = (23 - size) << shift
// size = 23 - (small_[23] >> shift)
size_t smallSize() const {
  constexpr auto shift = kIsLittleEndian ? 0 : 2;
  return maxSmallSize - (static_cast<size_t>(small_[maxSmallSize]) >> shift);
}
```

**巧妙之处**：`23 - size` 编码使得当 `size == 23`（全部用完）时，`small_[23] = 0`，即 null 终止符！不需要额外空间存 `\0`。

### 1.4 initSmall 的页面跨越优化

```cpp
// 源码：FBString.h:701
void initSmall(const Char* const data, const size_t size) {
  constexpr size_t kPageSize = 4096;
  const auto addr = reinterpret_cast<uintptr_t>(data);
  if (!kIsSanitize &&
      size && (addr ^ (addr + sizeof(small_) - 1)) < kPageSize) {
    // 输入数据完全在一个页面内 → 安全地一次性 memcpy 全部 24 字节
    // 即使 size < 23，也读取全部 24 字节（包括后面的垃圾数据）
    // 编译器将其优化为 4 条指令的 SIMD 序列
    std::memcpy(small_, data, sizeof(small_));
  } else {
    // 跨页或 size == 0 → 安全地只复制 size 字节
    if (size != 0) fbstring_detail::podCopy(data, data + size, small_);
  }
  setSmallSize(size);
}
```

**页面跨越检测的技巧**：`(addr ^ (addr + sizeof(small_) - 1)) < kPageSize` 检查起始地址和结束地址是否在同一页面（4KB 对齐块）内。如果在同一页面，可以安全地读取超出实际 size 的字节（不会 segfault），让编译器生成更高效的 SIMD 指令。

### 1.5 Large 模式的 COW 引用计数

```cpp
// 源码：FBString.h:475
struct RefCounted {
  std::atomic<size_t> refCount_;  // 引用计数（在 data_ 之前）
  Char data_[1];                  // 柔性数组成员

  static RefCounted* fromData(Char* p) {
    return static_cast<RefCounted*>(
      static_cast<void*>(
        static_cast<unsigned char*>(static_cast<void*>(p)) - getDataOffset()));
    // 从 data_ 指针反推出 RefCounted 头指针（减去 offsetof）
  }

  static void decrementRefs(Char* p) {
    auto const dis = fromData(p);
    size_t oldcnt = dis->refCount_.fetch_sub(1, std::memory_order_acq_rel);
    if (oldcnt == 1) {
      ::free(dis);  // 最后一个引用 → 释放整块内存
    }
  }
};
```

### 1.6 copy 的三种路径

```cpp
// 源码：FBString.h:647-684
void copySmall(const fbstring_core& rhs) {
  // 直接复制整个 24 字节结构体（一次 SIMD 操作）
  ml_ = rhs.ml_;
}

void copyMedium(const fbstring_core& rhs) {
  // Medium 是 eager copy：malloc 新内存，memcpy 数据
  auto const allocSize = goodMallocSize((1 + rhs.ml_.size_) * sizeof(Char));
  ml_.data_ = static_cast<Char*>(checkedMalloc(allocSize));
  fbstring_detail::podCopy(rhs.ml_.data_, rhs.ml_.data_ + rhs.ml_.size_ + 1, ml_.data_);
  ml_.size_ = rhs.ml_.size_;
  ml_.setCapacity(allocSize / sizeof(Char) - 1, Category::isMedium);
}

void copyLarge(const fbstring_core& rhs) {
  // Large 是 COW：只增加引用计数
  ml_ = rhs.ml_;  // 复制指针和元数据
  RefCounted::incrementRefs(ml_.data_);  // atomic fetch_add(1)
}
```

### 1.7 Small → Medium → Large 升级路径

```cpp
// 源码：FBString.h:825
void reserveSmall(size_t minCapacity, bool disableSSO) {
  if (!disableSSO && minCapacity <= maxSmallSize) {
    // 仍然在 Small 范围内 → 不做任何事
  } else if (minCapacity <= maxMediumSize) {
    // Small → Medium：malloc + memcpy + 设置新 category
    auto const allocSizeBytes = goodMallocSize((1 + minCapacity) * sizeof(Char));
    auto const pData = static_cast<Char*>(checkedMalloc(allocSizeBytes));
    fbstring_detail::podCopy(small_, small_ + smallSize() + 1, pData);
    ml_.data_ = pData;
    ml_.size_ = smallSize();
    ml_.setCapacity(allocSizeBytes / sizeof(Char) - 1, Category::isMedium);
  } else {
    // Small → Large：RefCounted::create + memcpy
    auto const newRC = RefCounted::create(&minCapacity);
    fbstring_detail::podCopy(small_, small_ + smallSize() + 1, newRC->data_);
    ml_.data_ = newRC->data_;
    ml_.size_ = smallSize();
    ml_.setCapacity(minCapacity, Category::isLarge);
  }
}
```

### 1.8 `mutableDataLarge()`：写时复制的触发点

```cpp
// 源码：FBString.h:760
Char* mutableDataLarge() {
  if (RefCounted::refs(ml_.data_) > 1) {  // 如果有其他引用者
    unshare();  // 复制一份独立副本（fork）
  }
  return ml_.data_;
}
```

**何时触发 COW fork**：任何修改 Large 字符串的操作（`operator[]` 非 const、`push_back`、`append` 等）都调用 `mutableData()` → `mutableDataLarge()` → 可能 `unshare()`。

---

## 2. F14 哈希表：Meta 自研的开放寻址 + SIMD 探测

F14 是 Folly 自己的哈希表实现，比 SwissTable（Abseil/libstdc++）更进一步：

### 2.1 Chunk-based 布局

### Chunk 布局与 SIMD 探测图
```
F14 Chunk 内存布局（128 字节，cache-line 对齐）：

┌──────────────────── 128 字节 Chunk ────────────────────────────────────┐
│                                                                        │
│  Tag 数组（16 字节，16 字节对齐）                                       │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐ │
│  │ T0 │ T1 │ T2 │ T3 │ T4 │ T5 │ T6 │ T7 │ T8 │ T9 │T10 │T11 │T12 │T13 │ OF │SENT│ │
│  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘ │
│   ├──── 14 个 H2 tag ────────────────────────┤│溢出│哨兵│                          │
│                                                                        │
│  每个 Tag 字节（8 位）：                                                │
│  ┌───┬──────────────┐                                                  │
│  │occ│  H2 (7 bit)  │  occupied=1 表示已占用                           │
│  └───┴──────────────┘                                                  │
│                                                                        │
│  Value 数组（14 个 slot）                                               │
│  ┌─────────┬─────────┬─────────┬─────────┬─ ··· ─┬─────────┐          │
│  │ slot 0  │ slot 1  │ slot 2  │ slot 3  │       │ slot 13 │          │
│  │ value   │ value   │ value   │ value   │       │ value   │          │
│  └─────────┴─────────┴─────────┴─────────┴─ ··· ─┴─────────┘          │
│                                                                        │
│  注意：Tag[14] 和 Tag[15] 不对应任何 slot → 实际可用 14 个值            │
└────────────────────────────────────────────────────────────────────────┘

SSE2 SIMD tag 匹配过程：

  步骤 1：加载 16 个 tag 到 128 位寄存器
  ┌──────────────────────────────────────────────────────────────────────┐
  │ XMM = _mm_load_si128(tagArray)                                       │
  │ [T0][T1][T2][T3][T4][T5][T6][T7][T8][T9][T10][T11][T12][T13][OF][SN]│
  └──────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
  步骤 2：广播目标 tag 到全部 16 字节
  ┌──────────────────────────────────────────────────────────────────────┐
  │ XMM2 = _mm_set1_epi8(target_tag)                                     │
  │ [TG][TG][TG][TG][TG][TG][TG][TG][TG][TG][TG] [TG] [TG] [TG][TG][TG]│
  └──────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
  步骤 3：逐字节比较 → 16 位 mask
  ┌──────────────────────────────────────────────────────────────────────┐
  │ result = _mm_cmpeq_epi8(XMM, XMM2)                                   │
  │ mask  = _mm_movemask_epi8(result)                                     │
  │                                                                       │
  │  示例：tag 匹配 slot 3 和 slot 11                                      │
  │  mask = 0b 10001000 00000000  → ctz(mask)=3 → 检查 slot[3]          │
  │                           1000 01000000  → ctz(mask)=11→ 检查 slot[11] │
  │  mask &= mask - 1 清除最低位，继续下一个候选                           │
  └──────────────────────────────────────────────────────────────────────┘
```
F14 将哈希表分为 **chunks**，每个 chunk 包含 16 个 tag 字节和最多 14 个 value slot：

```
一个 Chunk 的内存布局（128 字节对齐）：

Tag 数组（16 字节）:
  [tag0][tag1]...[tag15]
   每个 tag = 7 位 H2 哈希 + 1 位 occupied

Value 数组（14 个 slot × sizeof(value)）:
  [slot0][slot1]...[slot13]
  （slot14 和 slot15 不存在——tag 数组有 16 个，但只有 14 个 slot）

溢出指针：
  当 14 个 slot 全满时，通过链表指向溢出 chunk
```

**为什么 14 而不是 16？** tag 数组必须 16 字节对齐（SSE2 一次处理 16 字节），但 F14 需要 2 个 tag 位置用于溢出标记和哨兵。所以实际可用 slot = 16 - 2 = 14。

### 2.2 SIMD 探测

```cpp
// F14 使用 SSE2/NEON 进行 tag 匹配
#if FOLLY_SSE >= 2
__m128i ctrl = _mm_load_si128(tagArray);  // 一次加载 16 个 tag
__m128i match = _mm_set1_epi8(tag);       // 广播目标 tag
uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, match));
// mask 的每个置位位 = 一个候选 slot
while (mask) {
  int idx = __builtin_ctz(mask);
  // 检查 slot[idx] 是否真正匹配 key
  if (keys_equal(slot[idx], target_key)) return &slot[idx];
  mask &= mask - 1;  // 清除最低位
}
#endif
```

### 2.3 与 SwissTable 的对比

| 维度 | F14 | SwissTable |
|------|-----|-----------|
| 基本单位 | 128 字节 chunk（14 slots） | 连续 ctrl 数组 + 连续 slot 数组 |
| tag/ctrl | 16 字节对齐数组 | 1 字节 ctrl 交错或分离 |
| 溢出处理 | chunk 内链表 | 线性探测到下一个空位 |
| 缓存行为 | chunk 内连续访问 | 连续探测跨多个缓存行 |
| 负载因子 | ~87%（14/16） | ~87%（7/8） |

---

## 3. IOBuf：零拷贝链式缓冲区

IOBuf 是 Meta 网络栈（proxygen）的数据基石：

### 3.1 内部结构

```cpp
class IOBuf {
  IOBuf* next_;     // 链表指针（循环双向链表）
  IOBuf* prev_;
  uint8_t* data_;   // 数据指针
  size_t length_;   // 有效数据长度
  size_t capacity_; // 缓冲区容量
  SharedInfo* sharedInfo_;  // 引用计数 + 释放回调
  IOBufCookie cookie_;      // 内存来源标记
};
```

### 链式缓冲区与遍历图
```
IOBuf 循环双向链表示例（buf1->appendChain(buf2->appendChain(buf3))）：

                        ┌─────────────────────────────────────────┐
                        │             head = buf1                 │
                        │                 ↓                       │
  ┌─────────────────────┼──┐     ┌──────────────────┐    ┌──────┼─────────────┐
  │  ┌──────┐ ┌──────┐  │  │     │  ┌──────┐ ┌──────┐│    │ ┌──────┐ ┌──────┐ │
  │  │prev_ │ │next_ │──┼──┼────→│  │prev_ │ │next_ ││───→│ │prev_ │ │next_ │─┼─┐
  │  └──────┘ └──────┘  │  │  ┌──┼──┴──────┘ └──────┘│  ┌─┼─┴──────┘ └──────┘ │ │
  │                     │  │  │  │                    │  │ │ │                   │ │
  │     buf1            │  │  │  │      buf2          │  │ │ │     buf3          │ │
  │  ┌──────────────┐   │  │  │  │  ┌──────────────┐  │  │ │ │ ┌──────────────┐ │ │
  │  │data_ ─────────────→│  │  │  │data_ ─────────────→│  │ │ │data_ ─────────→│ │
  │  │length_: 1024 │   │  │  │  │  │length_: 512  │  │  │ │ │ │length_: 256  │ │ │
  │  │capacity_:1024 │   │  │  │  │  │capacity_: 512│  │  │ │ │ │capacity_: 256│ │ │
  │  │sharedInfo_    │   │  │  │  │  │sharedInfo_   │  │  │ │ │ │sharedInfo_   │ │ │
  │  └──────────────┘   │  │  │  │  └──────────────┘  │  │ │ │ └──────────────┘ │ │
  │                     │  │  │  │                    │  │ │ │                   │ │
  │  ┌──────────┐       │  │  │  │  ┌──────────┐     │  │ │ │ ┌──────────┐     │ │
  │  │ Heap buf │       │  │  │  │  │ Heap buf │     │  │ │ │ │ Heap buf │     │ │
  │  │ 1024 B   │       │  │  │  │  │ 512 B    │     │  │ │ │ │ 256 B    │     │ │
  │  └──────────┘       │  │  │  │  └──────────┘     │  │ │ │ └──────────┘     │ │
  └─────────────────────┼──┘  │  └──────────────────┘  │ │ └──────────────────┘ │
            ↑           │     │                        │ │          │            │
            │           │     └────────────────────────│─┘          │            │
            └───────────┼──────────────────────────────┘←───────────┘            │
                        └───────────────────────────────────────────────────────┘

链表结构：  buf1 ←→ buf2 ←→ buf3 ←→ buf1（循环）

链式遍历：
  for (auto& chunk : *buf1)  // begin=buf1, 沿 next_ 走直到回到 buf1
  ┌──────┐   next_   ┌──────┐   next_   ┌──────┐   next_
  │ buf1 │ ────────→ │ buf2 │ ────────→ │ buf3 │ ────────→ buf1（停止）
  │d[0..1K]          │d[0..512]         │d[0..256]
  └──────┘           └──────┘           └──────┘
  逻辑上：buf1.data[0..1023] + buf2.data[0..511] + buf3.data[0..255] = 连续字节流
```
### 3.2 链式缓冲区（chain）

```cpp
auto buf1 = IOBuf::create(1024);
auto buf2 = IOBuf::create(512);
buf1->appendChain(std::move(buf2));
// buf1 -> buf2 -> buf1 (循环链表)

// 遍历逻辑上连续但物理上分散的数据：
for (auto& chunk : *buf1) {
  process(chunk.data(), chunk.length());
}
```

### 3.3 零拷贝 clone

```cpp
auto shared = buf->clone();
// 只增加 sharedInfo_->refcount，不复制数据
// shared 和 buf 共享同一块内存
// 任何一方的修改都会影响另一方（需要外部同步）
```

### 3.4 SharedInfo 与自定义释放

```cpp
struct SharedInfo {
  std::atomic<size_t> refcount;
  FreeFunction freeFn;    // 自定义释放函数
  void* userData;          // 传递给 freeFn 的上下文
};
// 这允许 IOBuf 管理任意来源的内存：
// - 堆分配（freeFn = free）
// - mmap 映射（freeFn = munmap）
// - 外部只读内存（freeFn = no-op）
```

---

## 4. Future/Promise：Core + Executor 模型

### 4.1 Core 共享状态

```
Promise<T>  ──写入──→  Core<T>  ←──读取──  Future<T>
                         │
                    Try<T> result       (值或异常)
                    callback            (continuation)
                    Executor*           (调度器)
```

```cpp
template <typename T>
struct Core {
  Try<T> result_;                    // 值或异常
  std::function<void(Try<T>&&)> callback_;  // continuation
  Executor* executor_;               // 绑定的 Executor
  std::atomic<State> state_;         // 状态机：Start → OnlyResult → OnlyCallback → Done
  std::shared_ptr<RequestContext> context_;
};
```

### 4.2 状态机

```
Core<T> 状态机完整转换图：

                         Promise::setValue()
                    ┌─────────────────────────────────┐
                    │                                 ▼
              ┌─────────┐   Future::thenValue()  ┌──────────────┐
              │         │ ─────────────────────→ │              │
              │  Start  │                        │ OnlyCallback │
              │         │ ─────────────────────→ │              │
              └─────────┘                        └──────┬───────┘
                    │    Promise::setValue()              │
                    │                                 ▼
                    │                           ┌──────────────┐
                    │                           │     Done     │ ← callback(result)
                    │                           │   (terminal) │   触发后清理
                    │                           └──────────────┘
                    │                                 ▲
              ┌──────────────┐                        │
              │              │   Future::thenValue()  │
              │  OnlyResult  │ ─────────────────────→ ┘
              │              │
              └──────────────┘

路径 1（Promise 先设值）：
  Start ──setValue()──→ OnlyResult ──thenValue()──→ Done
                         （值已缓存，注册 callback 后立即触发）

路径 2（Future 先注册 callback）：
  Start ──thenValue()──→ OnlyCallback ──setValue()──→ Done
                         （callback 已就绪，设值后立即触发）

无论哪方先到，callback 都保证被触发——状态机消除竞态
```

### 4.3 SemiFuture vs Future

```cpp
// SemiFuture：未绑定 Executor，不能调用 .then()
// 必须先通过 .via(executor) 转换为 Future
SemiFuture<int> sf = makeSemiFuture(42);
Future<int> f = std::move(sf).via(&executor);

// Future：已绑定 Executor，可以链式 continuation
auto result = std::move(f)
    .thenValue([](int v) { return v * 2; })
    .thenValue([](int v) { return v + 1; });
```

**为什么区分？** SemiFuture 防止用户意外在错误的 Executor 上执行 continuation。必须显式选择 Executor。

---

## 5. Synchronized&lt;T&gt;：类型安全的锁守卫
### 加锁访问模式图
```
Synchronized&lt;T&gt; 内部结构与加锁流程：

  Synchronized&lt;vector&lt;int&gt;&gt; 数据结构：
  ┌─────────────────────────────────────────┐
  │  mutex_:  SharedMutex                   │
  │  datum_:  vector&lt;int&gt;  {1, 2, 3}        │
  └─────────────────────────────────────────┘

  wlock() 写锁获取流程：
  ─────────────────────────────────────────────────────────────
    调用者                         Synchronized
      │                                │
      │── wlock() ───────────────────→ │
      │                                ├──→ std::unique_lock(mutex_)
      │                                │       │
      │                                │       ▼
      │                                │    ┌──────────┐
      │                                │    │ mutex_   │ locked (独占)
      │                                │    └──────────┘
      │                                │       │
      │                                │       ▼
      │                                │    构造 LockedPtr(this, lock)
      │                                │       │
      │←── LockedPtr ───────────────────       │
      │    (持有独占锁，可读写 datum_) │       │
      │                                │       │
    lp->push_back(4)   ← 通过 operator-> 直接访问 datum_
      │                                │       │
      │ ~LockedPtr() ← 作用域结束       │       │
      │                                │       ▼
      │                                │    ┌──────────┐
      │                                │    │ mutex_   │ unlocked
      │                                │    └──────────┘
  ─────────────────────────────────────────────────────────────

  rlock() 读锁获取流程（可并发读）：
  ─────────────────────────────────────────────────────────────
    线程 A                  Synchronized               线程 B
      │                         │                        │
      │── rlock() ────────────→ │                        │
      │                         ├──→ shared_lock(mutex_) │
      │←── LockedPtr (shared) ──│                        │
      │                         │                        │
      │  读取 datum_ ...        │ ←── rlock() ──────────│
      │                         ├──→ shared_lock(mutex_) │
      │                         │←── LockedPtr (shared) ─│
      │                         │                        │
      │  读取 datum_ ...        │       读取 datum_ ...  │  ← 并发读！
      │                         │                        │
      │ ~LockedPtr()            │                        │
      │                         │   ~LockedPtr()         │
  ─────────────────────────────────────────────────────────────

  关键保证：编译期不可能绕过锁直接访问 datum_
    datum_ 是 private 成员 → 只有通过 wlock()/rlock() 返回的 LockedPtr 才能访问
```

```cpp
template <typename T, typename Mutex = SharedMutex>
class Synchronized {
  mutable Mutex mutex_;
  T datum_;

public:
  // 写锁
  LockedPtr wlock() {
    return LockedPtr(this, std::unique_lock(mutex_));
  }
  // 读锁
  LockedPtr rlock() const {
    return LockedPtr(this, std::shared_lock(mutex_));
  }
  // 带超时的写锁
  std::optional<LockedPtr> wlock(Duration timeout) {
    std::unique_lock lk(mutex_, timeout);
    if (!lk.owns_lock()) return std::nullopt;
    return LockedPtr(this, std::move(lk));
  }
};
```

`LockedPtr` 持有锁并提供 `operator->` 和 `operator*` 访问数据。锁在 `LockedPtr` 析构时释放。**不可能在不加锁的情况下访问数据**——编译器直接阻止。

---

## 与标准库对比

| 组件 | Folly | 标准库 | 差异 |
|------|-------|--------|------|
| string | fbstring 三级 (Small/Medium/Large COW) | std::string 两级 (Small/Long) | fbstring SSO=23 vs 15/22 |
| hash_map | F14 (chunk + SIMD) | std::unordered_map (链式) 或 SwissTable | F14 缓存更友好 |
| byte_buffer | IOBuf (引用计数 + 链式) | vector&lt;char&gt; (拷贝语义) | IOBuf 零拷贝 |
| async | Future/Promise + Executor | std::future (阻塞) | Folly 全 continuation |
| thread_safe | Synchronized&lt;T&gt; (RAII) | 手动 mutex + lock_guard | 编译期强制加锁 |
| hash | SpookyHashV2 (128-bit) | std::hash | 更高质量分布 |
| callable | folly::Function (SBO 24B, move-only) | std::function (SBO 24B, copy-only) | 支持 move-only |
