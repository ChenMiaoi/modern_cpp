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
