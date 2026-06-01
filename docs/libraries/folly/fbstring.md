# Folly fbstring：三级存储的字节级实现

> 源码路径：`references/impl/folly/folly/FBString.h`

fbstring 在 64 位系统上占 24 字节（与 libc++ string 相同），但使用**三级**而非两级存储。这是 Folly 最精巧的数据结构之一。

## 存储布局

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
└────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────── Large（≥ 255 字节，COW）────────────────────┐
│                                                                            │
│   字节:  0 ─────── 7   8 ──────── 15  16 ──────── 23                      │
│  ┌──────────────┬────────────────┬────────────────┐                        │
│  │  data_ (ptr) │  size_         │  capacity_     │                        │
│  │  Char* 8B    │  size_t 8B     │  size_t 8B     │                        │
│  └──────┬───────┴────────────────┴────────────────┘                        │
│         │     bit[62] = 1 标记 Large                                       │
│         └──→ data_ 指向 RefCounted::data_[0]                               │
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
```

## 类别编码（源码）

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

## Small 模式：SSO 容量 23 字节

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

## initSmall 的页面跨越优化

```cpp
// 源码：FBString.h:701
void initSmall(const Char* const data, const size_t size) {
  constexpr size_t kPageSize = 4096;
  const auto addr = reinterpret_cast<uintptr_t>(data);
  if (!kIsSanitize &&
      size && (addr ^ (addr + sizeof(small_) - 1)) < kPageSize) {
    // 输入数据完全在一个页面内 → 安全地一次性 memcpy 全部 24 字节
    std::memcpy(small_, data, sizeof(small_));
  } else {
    // 跨页或 size == 0 → 安全地只复制 size 字节
    if (size != 0) fbstring_detail::podCopy(data, data + size, small_);
  }
  setSmallSize(size);
}
```

**页面跨越检测**：`(addr ^ (addr + sizeof(small_) - 1)) < kPageSize` 检查起始地址和结束地址是否在同一 4KB 页面内。如果在，可以安全读取全部 24 字节（包括超出 size 的垃圾数据），让编译器生成更高效的 SIMD 指令。

## Large 模式的 COW 引用计数

```cpp
// 源码：FBString.h:475
struct RefCounted {
  std::atomic<size_t> refCount_;  // 引用计数（在 data_ 之前）
  Char data_[1];                  // 柔性数组成员

  static void decrementRefs(Char* p) {
    auto const dis = fromData(p);
    size_t oldcnt = dis->refCount_.fetch_sub(1, std::memory_order_acq_rel);
    if (oldcnt == 1) {
      ::free(dis);  // 最后一个引用 → 释放整块内存
    }
  }
};
```

```
RefCounted 内存布局（Large 模式）:

  data_ 指针 ──→ RefCounted::data_[0]
                 ↓
  堆内存：
  偏移:  -8 (或 -N)    0   1   2       N-1
        ┌──────────┬───┬───┬───┬──────┬────┐
        │ refCount │ d0│ d1│ d2│ ...  │ \0 │
        │ atomic   │   │   │   │      │    │
        └──────────┴───┴───┴───┴──────┴────┘
        ↑                ↑
   getDataOffset()    data_[0]

  多个 Large fbstring 的 data_ 指向同一块 → 共享 refCount
  decrementRefs: fetch_sub(1) == 1 时 free 整块内存
```

## copy 的三种路径

```cpp
void copySmall(const fbstring_core& rhs) {
  ml_ = rhs.ml_;  // 直接复制整个 24 字节
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
  ml_ = rhs.ml_;
  RefCounted::incrementRefs(ml_.data_);  // atomic fetch_add(1)
}
```

## COW 触发点

```cpp
// 源码：FBString.h:760
Char* mutableDataLarge() {
  if (RefCounted::refs(ml_.data_) > 1) {  // 有其他引用者
    unshare();  // 复制一份独立副本（fork）
  }
  return ml_.data_;
}
```

**何时触发 COW fork**：任何修改 Large 字符串的操作（`operator[]` 非 const、`push_back`、`append` 等）都调用 `mutableData()` → `mutableDataLarge()` → 可能 `unshare()`。

## 与标准库 string 的对比

| 实现 | sizeof | SSO 容量 | Medium 起点 | Large 模式 |
|------|--------|---------|------------|-----------|
| fbstring | 24 | 23 | 24 | COW (≥255) |
| libc++ | 24 | 22 | 23 | eager copy |
| libstdc++ | 32 | 15 | 16 | eager copy |
| MSVC | 32 | 15 | 16 | eager copy |

**fbstring 的优势**：最大的 SSO 容量（23 vs 22/15）、Large 模式的 COW 节省大字符串复制开销。

**fbstring 的劣势**：COW 在多线程环境下的 atomic 引用计数有竞争开销；COW 与 C++11 的引用稳定性要求不完全兼容（与 libstdc++ COW string 相同的问题）。
