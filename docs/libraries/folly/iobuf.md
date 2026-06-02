---
title: Folly IOBuf
topic: libraries
feature: iobuf
standard: N/A
status_checked_at: 2026-06-02
implementation:
  folly:
    path: references/impl/folly/folly/IOBuf.h
    symbols:
      - IOBuf
      - SharedInfo
exercises: []
solutions: []
---
# Folly IOBuf：零拷贝链式缓冲区

> 源码路径：`references/impl/folly/folly/IOBuf.h`

IOBuf 是 Meta 网络栈（proxygen）的数据基石——零拷贝、引用计数、链式缓冲区。

## 内部结构

```cpp
class IOBuf {
  IOBuf* next_;     // 循环双向链表
  IOBuf* prev_;
  uint8_t* data_;   // 数据指针
  size_t length_;   // 有效数据长度
  size_t capacity_; // 缓冲区容量
  SharedInfo* sharedInfo_;  // 引用计数 + 释放回调
};
```

## 链式缓冲区

```
IOBuf 循环双向链表（buf1->appendChain(buf2->appendChain(buf3))）：

  ┌──────┐   next_   ┌──────┐   next_   ┌──────┐   next_
  │ buf1 │ ────────→ │ buf2 │ ────────→ │ buf3 │ ────────→ buf1（循环）
  │d[0..1K]          │d[0..512]         │d[0..256]
  └──────┘           └──────┘           └──────┘

  逻辑上：buf1.data[0..1023] + buf2.data[0..511] + buf3.data[0..255] = 连续字节流
```

```cpp
auto buf1 = IOBuf::create(1024);
auto buf2 = IOBuf::create(512);
buf1->appendChain(std::move(buf2));
// buf1 -> buf2 -> buf1 (循环链表)

for (auto& chunk : *buf1) {
  process(chunk.data(), chunk.length());
}
```

## 零拷贝 clone

```cpp
auto shared = buf->clone();
// 只增加 sharedInfo_->refcount，不复制数据
// shared 和 buf 共享同一块内存
```

## SharedInfo 与自定义释放

```cpp
struct SharedInfo {
  std::atomic<size_t> refcount;
  FreeFunction freeFn;    // 自定义释放函数
  void* userData;
};
// 允许 IOBuf 管理任意来源的内存：
// - 堆分配（freeFn = free）
// - mmap 映射（freeFn = munmap）
// - 外部只读内存（freeFn = no-op）
```

## 与 std::vector\<char\> 的对比

| 维度 | IOBuf | vector\<char\> |
|------|-------|---------------|
| 拷贝 | **零拷贝（引用计数）** | O(n) 深拷贝 |
| 拼接 | O(1) 链表操作 | O(n) 数据搬移 |
| 随机访问 | 需要处理链表边界 | O(1) |
| 适用场景 | 网络 I/O、协议栈 | 通用字节缓冲 |

## 用户 API

用户通常通过 `IOBuf::create`、`appendChain`、`clone`、`coalesce`、`data()/length()` 等入口使用 IOBuf；现有正文已经解释了链式缓冲区和零拷贝 clone。

## 标准语义

IOBuf **不是**标准库类型，不满足 `std::ranges::range`、`std::contiguous_iterator` 或 `TriviallyCopyable` 等任何标准概念。它与标准设施的语义边界如下：

| 标准设施 | IOBuf 对应角色 | 语义差异 |
|----------|---------------|---------|
| `std::vector<std::byte>` | 连续字节缓冲 | `vector` 拥有内存且深拷贝；IOBuf 引用计数零拷贝，但单个 chunk 内的 `[data(), tail())` 才是连续区间 |
| `std::span<std::byte>` | 单 chunk 非拥有视图 | `span` 是编译期大小或动态大小的平凡视图；IOBuf 额外携带 headroom/tailroom、链表、引用计数 |
| `std::string` / `std::string_view` | 文本载荷 | IOBuf 存储原始字节，不保证 null 终止；`toString()` 需要显式调用 |
| `std::shared_ptr<T>` | 共享所有权 | `shared_ptr` 控制对象生命周期；IOBuf 的引用计数只管**底层缓冲区**，IOBuf 对象本身没有侵入式引用计数 |

关键语义约束：
- **写前必须 unshare**：IOBuf 的 `writableData()` 不检查独占性——调用者必须先调用 `unshare()` 保证可写。这与 COW 的 `std::string`（隐式 fork）不同。
- **链表不透明**：`coalesce()` 之前，逻辑上连续的字节流可能分布在多个不连续的 chunk 中，标准算法无法直接遍历。
- **无所有权语义**：`wrapBuffer()` 创建的 IOBuf 不拥有底层内存，`sharedInfo_` 为 `nullptr`，`isSharedOne()` 永远返回 `true`。调用者必须保证 buffer 在所有 IOBuf 销毁前有效。

## 对象布局

上文已经给出 `IOBuf` 头部字段和循环双向链表；后续补 `SharedInfo`、headroom/tailroom 与外部缓冲包装的布局图。

## 核心源码路径

本文开头已给出 `IOBuf.h`；后续补 `IOBuf.cpp`、`takeOwnership`、`coalesce`、`cloneOne` 等入口函数路径。

## 核心类 / 函数

### `IOBuf`（`IOBuf.h:292`）

主类型，56 字节（`sizeof(IOBuf) <= 56` 由 `HeapStorage` 的 `static_assert` 保证）。

```
偏移    字段             类型              说明
─────────────────────────────────────────────────────
 0      length_          size_t            有效数据长度
 8      data_            uint8_t*          数据起始指针
16      capacity_        size_t            缓冲区总容量
24      buf_             uint8_t*          缓冲区起始指针
32      next_            IOBuf*            链表后继（循环，永不为 null）
40      prev_            IOBuf*            链表前驱（循环，永不为 null）
48      sharedInfo_      SharedInfo*       引用计数块（可能为 null）
```

### `SharedInfo`（`IOBuf.h:2120`）

```cpp
struct SharedInfo {
  FreeFunction freeFn{nullptr};           // 自定义释放回调
  void* userData{nullptr};                // 回调用户数据
  SharedInfoObserverEntryBase* observerListHead{nullptr};
  std::atomic<uint32_t> refcount{1};      // 原子引用计数
  bool externallyShared{false};           // 标记外部共享
  StorageType storageType = kInvalid;     // 存储类型枚举
  MicroSpinLock observerListLock{0};      // observer 链表锁
};
```

`StorageType` 枚举决定 `SharedInfo` 自身的释放方式：
- `kAllocated`：独立 `new` 分配，`delete` 释放
- `kHeapFullStorage`：与 IOBuf + 数据一起分配，引用计数归零后由 `HeapStorage` 管理释放
- `kExtBuffer`：嵌入在数据缓冲区尾部（`SharedInfo` 紧跟 `malloc` 的 buffer 末尾）

### 工厂函数

| 函数 | 语义 |
|------|------|
| `create(capacity)` | ≤1024 字节走 `createCombined`（IOBuf+SharedInfo+数据单次分配）；否则 `createSeparate` |
| `createCombined(capacity)` | 单次 `malloc`：`HeapFullStorage` + 数据；`static_assert(sizeof(HeapFullStorage) <= 64)` |
| `createSeparate(capacity)` | IOBuf 对象与数据缓冲分开分配 |
| `createChain(total, max)` | 创建链式 IOBuf，每个 chunk ≤ `max` |
| `takeOwnership(buf, cap, ...)` | 接管外部已分配缓冲区，指定 `FreeFunction` |
| `wrapBuffer(buf, cap)` | 非拥有包装，`sharedInfo_ = nullptr`，不管理内存 |
| `copyBuffer(data, size, ...)` | 深拷贝：`create` + `memcpy` |

### 链操作

| 函数 | 语义 | 复杂度 |
|------|------|--------|
| `appendToChain(unique_ptr)` | 将另一条链拼接到本链尾部 | O(1) 指针操作 |
| `insertAfterThisOne(unique_ptr)` | 在当前节点后插入 | O(1) |
| `unlink()` | 从链中摘除自身，返回 `unique_ptr` | O(1) |
| `pop()` | 摘除并返回除自身外的其余链 | O(1) |
| `separateChain(head, tail)` | 摘除子链 `[head, tail]` | O(1) |

### 共享与写安全

| 函数 | 语义 |
|------|------|
| `clone()` | 遍历链，对每个 chunk 调用 `cloneOne`（`fetch_add(1)`） |
| `cloneOne()` | 仅当前 chunk：`refcount.fetch_add(1, relaxed)` + 新建 IOBuf 对象 |
| `unshare()` | 若链中有共享 chunk，`coalesceSlow()` 合并为连续缓冲 |
| `unshareOne()` | 仅当前 chunk：分配新缓冲区 + `memcpy` + 旧引用计数减一 |
| `isSharedOne()` | `!sharedInfo_ \|\| externallyShared \|\| refcount > 1` |
| `coalesce()` | 将整条链合并为单个连续缓冲，返回 `ByteRange` |

## 关键算法

### 1. 链拼接（`appendToChain`，IOBuf.cpp:774）

```
链 A: [A1]⇄[A2]⇄[A1]    链 B: [B1]⇄[B2]⇄[B1]

appendToChain(B1) 后：
  A1.prev_→A2.next_→B1.next_→B2.next_→A1（循环）

  关键操作：4 次指针赋值，无数据拷贝
  A2.next_ = B1;  B1.prev_ = A2;
  B2.next_ = A1;  A1.prev_ = B2;
```

### 2. 零拷贝 clone（`cloneOneImpl`，IOBuf.cpp:802）

```cpp
unique_ptr<IOBuf> IOBuf::cloneOneImpl(mr) const {
  if (sharedInfo_) {
    sharedInfo_->refcount.fetch_add(1, std::memory_order_relaxed);
  }
  // 只分配新的 IOBuf 对象（HeapStorage），共享同一缓冲区
  return unique_ptr<IOBuf>{new (&storage->buf) IOBuf(
      InternalConstructor(), sharedInfo_, buf_, capacity_, data_, length_)};
}
```

`clone()` 遍历链上每个 chunk 调用 `cloneOneImpl`，再用 `appendToChain` 拼接。

### 3. coalesce（`coalesceAndReallocate`，IOBuf.cpp:1041）

```
coalesce 前：  [Buf1:head|data1|tail] → [Buf2:head|data2|tail] → [Buf3:head|data3|tail]

coalesce 后：  [head₁|data1+data2+data3|tail₃]  单一连续缓冲

步骤：
1. 计算 newLength = Σ length_i
2. allocExtBuffer(newHeadroom + newLength + newTailroom)
3. 从 this 开始遍历链，memcpy 每个 chunk 的数据到新缓冲区
4. decrementRefcount()（旧缓冲区）
5. separateChain(next_, end->prev_)（摘除旧节点，立即析构）
```

### 4. 引用计数释放（`decrementRefcount`，IOBuf.cpp:1088）

```
decrementRefcount():
  if (!sharedInfo_) return;                    // wrapBuffer 的 IOBuf 无 refcount
  if (refcount.load(acquire) > 1) {            // 快速路径：非最后引用
    if (fetch_sub(1, acq_rel) > 1) return;     // 还有其他引用，直接返回
  }
  // 最后一个引用
  freeExtBuffer();                             // 调用 freeFn(buf_, userData) 或 free(buf_)
  SharedInfo::releaseStorage(this, type, info); // 释放 SharedInfo 自身
```

`freeExtBuffer` 根据 `freeFn` 是否为空选择释放方式：
- `freeFn != nullptr`：调用 `freeFn(buf_, userData)`（支持 mmap、外部内存等）
- `freeFn == nullptr && userData != 0`：`sizedFree(buf_, size)`（利用 `goodMallocSize` 加速）
- 否则：`free(buf_)`

## ABI 约束

IOBuf 不提供标准库式的 ABI 承诺。其 ABI 约束来源：

**对象布局耦合**：
- `sizeof(IOBuf) <= 56` 由 `HeapStorage` 的 `static_assert` 约束；字段顺序和大小直接被 `HeapFullStorage`（IOBuf + SharedInfo + data 单次分配）和 `HeapPrefix`（`offsetof(HeapStorage, buf)`）依赖。
- `SharedInfo` 的 `StorageType` 枚举值和布局被 `releaseStorage()` 中的指针算术直接使用（如 `reinterpret_cast` 计算 `HeapFullStorage` 起始地址）。

**回调签名**：
- `FreeFunction = void(*)(void* buf, void* userData)` 是 C ABI 兼容的函数指针，改签名会破坏所有 `takeOwnership` 调用者。
- `io_buf_alloc_cb` / `io_buf_free_cb` 是 weak symbol，其签名 `void(void*, size_t)` 也被外部程序定义。

**operator new/delete 重载**：
- `IOBuf::operator new(size_t)` 分配 `HeapStorage` 而非裸 `IOBuf`；`operator delete` 通过 `offsetof(HeapStorage, buf)` 回退到 `HeapStorage`。子类如果重写 new/delete 会破坏 `createCombined` 的假设。

**API 演进策略**：
- Folly 以**源码兼容**为目标，不保证跨版本的二进制兼容。
- 新增工厂函数（如 `cloneCoalescedAsValue`）和 PMR 重载是主要的演进方式。
- 已废弃的 `prependChain` / `appendChain` 保留为转发包装。

## 异常安全

IOBuf 的异常安全保证因操作而异：

**析构函数与链操作：noexcept**
- `~IOBuf()`、`decrementRefcount()`、`freeExtBuffer()` 均标记 `noexcept`。
- `FreeFunction` 文档要求"不得抛出异常"——若回调违反此约定，`std::terminate` 会被调用。
- `move constructor`、`move assignment` 均为 `noexcept`。

**分配失败：抛 `std::bad_alloc`，强异常保证**
- `create()`、`createCombined()`、`createSeparate()`、`copyBuffer()`：分配失败直接抛异常，无副作用。
- `takeOwnership(buf, cap, freeFn, userData, freeOnError=true)`：若 `SharedInfo` 的 `new` 抛异常，`ScopeGuard` 会调用 `freeFn(buf, userData)` 回滚——即**强异常保证**（`IOBuf.cpp:500`）。
- `coalesce()`、`coalesceAndReallocate()`：新缓冲区分配失败后，旧链状态不变——强异常保证。
- `reserveSlow()`：若 `malloc` 失败抛 `bad_alloc`，原缓冲区不变。

**边界情况**：
- `coalesceSlow(maxLength)` 中若 `maxLength > computeChainDataLength()`，抛 `std::overflow_error`。
- `checked_add` / `checkedMath` 溢出时抛 `std::bad_alloc` 或 `std::length_error`。
- `unshareChained()` 若链中有共享 chunk 会调用 `coalesceSlow()`，后者可能因 `bad_alloc` 失败——原链不变。

## iterator / reference invalidation

IOBuf 的失效规则与 `std::vector` 不同——它没有"所有迭代器失效"的统一语义，因为链结构和缓冲区生命周期是分离的。

| 操作 | `data()` 指针 | chunk Iterator | 链表指针（`next()`/`prev()`） | 外部 `sharedInfo_` |
|------|-------------|---------------|-------------------------------|------------------|
| `cloneOne()` / `clone()` | 不变 | N/A | 新链独立 | 不变（refcount 加一） |
| `appendToChain(other)` | 不变 | 不变 | **原 `next_` 链上所有 IOBuf 的 `prev_` 被重写** | 不变 |
| `unlink()` / `pop()` | 不变 | 不变 | 被摘除节点的 `next_`/`prev_` 指向自身；原链前后节点互相指向 | 不变 |
| `separateChain(h, t)` | 不变 | 不变 | 子链两端节点和原链断开处的指针被修改 | 不变 |
| `coalesce()` | **失效**（新缓冲区） | **所有非 head 节点被析构** | **链变为单节点** | 旧 `sharedInfo_` 递减，新 `sharedInfo_` 指向新缓冲 |
| `unshareOne()` | **失效**（新 `memcpy`） | 不变 | 不变 | 旧 `sharedInfo_` 递减 |
| `unshare()` (链式) | **失效**（内部调用 `coalesceSlow`） | **所有非 head 节点被析构** | 链变为单节点 | 同 `coalesce` |
| `advance(n)` / `retreat(n)` | **偏移**（`data_ += n`） | 不变 | 不变 | 不变 |
| `prepend(n)` / `trimStart(n)` | **偏移**（`data_ -= n` 或 `+= n`） | 不变 | 不变 | 不变 |
| `append(n)` / `trimEnd(n)` | 不变（只改 `length_`） | 不变 | 不变 | 不变 |
| `trimWritableTail(n)` | 不变（只改 `capacity_`） | 不变 | 不变 | 不变 |
| `reserve()` | 可能失效（`reserveSlow` 重新分配） | 不变 | 不变 | 旧 `sharedInfo_` 可能被替换 |
| `clear()` | 重置到 `buffer()` | 不变 | 不变 | 不变 |

**关键规则**：
- `coalesce()` / `unshare()` 后，之前保存的任何 `data()` 指针、`ByteRange` 视图、`iovec` 数组**全部失效**。
- 链操作（`appendToChain`、`unlink`）不会使 `data()` 指针失效，但会使**遍历链的代码**中保存的其他节点指针失效。
- `IOBuf::Iterator` 是前向迭代器，在 `coalesce()` / `unshare()`（链式）后**必然失效**。

## 性能模型

### 零拷贝共享 vs 最终 coalesce

IOBuf 的核心取舍：延迟数据拷贝以换取 I/O 路径上的零拷贝性能，但最终消费数据时（如解析协议头）必须 coalesce。

| 场景 | 推荐策略 | 原因 |
|------|---------|------|
| 网络收包后直接转发 | `clone()` + 发送 | 全程零拷贝，最优 |
| 网络收包后需要解析 | `gather(needed)` 部分 coalesce | 只合并需要连续访问的前 N 字节 |
| 多次 append 后一次性发送 | 不 coalesce，用 `getIov()` + `writev` | 保持链式结构，利用 scatter-gather I/O |
| 需要传给要求连续内存的 API | `coalesce()` 或 `cloneCoalesced()` | 不可避免的拷贝 |

### 链长度的开销

- **遍历开销**：`computeChainDataLength()`、`countChainElements()` 均为 O(N)，N = 链长度。
- **cache 局部性**：每个 IOBuf 对象在堆上独立分配（`HeapStorage`），链上不同 chunk 通常不相邻，遍历时会有 L1/L2 cache miss。链越长，miss 越多。
- **coalesce 的 memcpy 成本**：与总数据量线性相关。对于 10 个 1KB chunk 的链，coalesce 需要 memcpy ~10KB 数据 + 分配新缓冲区。

### 引用计数的原子操作开销

| 操作 | 原子指令 | 内存序 |
|------|---------|--------|
| `cloneOne` | `fetch_add(1)` | `relaxed`（最轻） |
| `decrementRefcount` | `load(acquire)` + `fetch_sub(1, acq_rel)` | 较重 |
| `isSharedOne` | `load(acquire)` | 轻量读 |

快速路径优化：`decrementRefcount` 先用 `load(acquire)` 检查 `refcount > 1`，只在非最后引用时才执行 `fetch_sub`——避免在非共享场景下的 `acq_rel` 开销。

### 自定义释放回调的额外开销

- `FreeFunction` 是函数指针调用，无法被内联——相比 `free(buf_)` 多一次间接调用。
- `takeOwnership(SIZED_FREE)` 路径使用 `folly::sizedFree`（利用 `goodMallocSize` 减少 jemalloc 内部查找），比裸 `free` 略快。
- `HeapFullStorage` 模式（`createCombined`）：IOBuf + SharedInfo + data 单次分配/释放，减少一次 `malloc`/`free` 对。对于 ≤1024 字节的小缓冲区，这是默认行为。

## libstdc++ vs libc++ vs MSVC

IOBuf 是 Folly 专有类型，不涉及标准库实现差异。此处对比其与标准库缓冲设施的设计取舍：

| 维度 | `IOBuf` | `std::vector<std::byte>` | `std::span<std::byte>` | `std::string` (libstdc++) |
|------|---------|--------------------------|------------------------|--------------------------|
| 所有权 | 引用计数（可共享） | 独占（深拷贝） | 非拥有视图 | COW (≤GCC 5) 或独占 |
| 内存布局 | 链式 chunk，每 chunk 可独立分配 | 单连续块 | 无存储（指针+大小） | SSO + 单连续块 |
| 拼接 | O(1) 链表操作 | O(n) 拷贝 | 不适用 | O(n) 或 COW |
| 零拷贝 | `clone()` 只加引用计数 | 不支持 | 不涉及所有权 | COW 模式可零拷贝 |
| 线程安全 | refcount 原子；IOBuf 对象本身不加锁 | 无 | 无 | refcount 原子（COW） |
| 可写保证 | 调用者须先 `unshare()` | 独占即为可写 | 不保证可写 | COW 模式隐式 `unshare` |
| 适用场景 | 网络 I/O、协议栈、零拷贝管道 | 通用字节容器 | 只读/借用视图 | 文本字符串 |

**标准库实现差异不影响 IOBuf**——IOBuf 自定义了 `operator new/delete`、内存分配和引用计数，完全绕开标准库的分配器和容器语义。不同平台（Linux/macOS/Windows）上 IOBuf 的行为一致，差异仅在于底层 `malloc` 实现（jemalloc vs tcmalloc vs 系统 malloc）对 `goodMallocSize` / `xallocx` 等 Folly 内存工具的支持程度。

## 最小复现代码

```cpp
#include <folly/io/IOBuf.h>

int main() {
  auto buf = folly::IOBuf::create(128);
  auto copy = buf->clone();
  return copy->capacity() > 0 ? 0 : 1;
}
```

## 编译 / 反汇编 / benchmark 证据

### Benchmark 结果（源码：`folly/io/test/IOBufBenchmark.cpp`）

以下数据来自 `folly::runBenchmarks`，`--bm_min_iters 100000`：

```
操作                                 时间/iter     吞吐量
──────────────────────────────────────────────────────────
createAndDestroy (10B)               17.42 ns     57.41 M/s
cloneOne                             23.73 ns     42.14 M/s
cloneOneInto (避免堆分配)            19.08 ns     52.40 M/s
clone (链式)                         24.92 ns     40.13 M/s
cloneInto (避免堆分配)               21.74 ns     45.99 M/s
move                                 8.61 ns     116.17 M/s
copy (拷贝构造=clone)                21.23 ns     47.11 M/s
cloneCoalescedBaseline               201.31 ns     4.97 M/s
  (clone + coalesce 分开)
cloneCoalesced (优化版)              36.21 ns     27.62 M/s  ← 5.56× 快
takeOwnership                        36.01 ns     27.77 M/s
──────────────────────────────────────────────────────────
createAndDestroyMulti(64B)           32.74 μs     30.54 K/s
createAndDestroyMulti(4096B)         70.16 μs     14.25 K/s
createAndDestroyMulti(10240B)        83.84 μs     11.93 K/s
createAndDestroyMulti(16384B)        93.03 μs     10.75 K/s
```

### 关键发现

**零拷贝 clone 的优势**：`cloneOne`（24 ns）vs `copyBuffer`（需要 `malloc` + `memcpy`，约 30-50 ns 取决于数据量）。对于典型 1500 字节的网络 MTU 包，零拷贝节省约 50-100 ns/包。

**`cloneCoalesced` vs `clone` + `coalesce`**：优化后的 `cloneCoalescedAsValue`（36 ns）比 baseline（201 ns）快 5.56 倍——原因是避免了中间链的堆分配（每个 `cloneOne` 需要一次 `HeapStorage` 分配）。这个差距在链越长时越大。

**`cloneOneInto` vs `cloneOne`**：复用栈上 IOBuf 对象（19 ns vs 24 ns）节省一次 `HeapStorage` 分配/释放。在热路径中值得使用。

**`createAndDestroyMulti` 的分配器跳变**：从 4096B 到 5120B 有一个明显的耗时跳变（70 μs → 80 μs），对应 jemalloc 的 size class 边界（4KB slab → 8KB slab）。

## cpplings 练习入口

- [`span1` — std::span 非拥有视图](../../../exercises/cpp20/span1.cpp)
- [`cachefriendly1` — 缓存友好的数据结构](../../../exercises/topics/cachefriendly1.cpp)
- [`perf1` — 性能优化技巧：缓存友好与 string_view](../../../exercises/topics/perf1.cpp)
