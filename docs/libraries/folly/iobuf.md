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

待补：补上 IOBuf 作为非标准网络缓冲抽象时，与 `std::vector<std::byte>` / `std::span` 这类标准设施的语义边界。

## 对象布局

上文已经给出 `IOBuf` 头部字段和循环双向链表；后续补 `SharedInfo`、headroom/tailroom 与外部缓冲包装的布局图。

## 核心源码路径

本文开头已给出 `IOBuf.h`；后续补 `IOBuf.cpp`、`takeOwnership`、`coalesce`、`cloneOne` 等入口函数路径。

## 核心类 / 函数

待补：统一整理 `IOBuf`、`SharedInfo`、`create`、`appendChain`、`clone`、`coalesce`、`unshare`。

## 关键算法

待补：补上链拼接、零拷贝共享、需要时 coalesce、引用计数减到零释放外部内存的关键路径。

## ABI 约束

待补：说明 IOBuf 主要受对象布局、回调签名与头文件 API 演进约束，而不是标准库式 ABI 承诺。

## 异常安全

待补：补充分配失败、外部缓冲接管失败、coalesce 迁移失败时的保证等级。

## iterator / reference invalidation

待补：明确链重组、`unshare`、`coalesce`、`trim` 后 `data()` 指针、chunk 视图与外部引用的失效边界。

## 性能模型

待补：补上“零拷贝共享 vs 最终 coalesce”的取舍、链长度增长带来的分支与 cache 成本，以及自定义释放回调的额外开销。

## libstdc++ vs libc++ vs MSVC

待补：这里更多是与标准库设施对照——`IOBuf` 相对 `std::vector<std::byte>`、`std::span`、`std::string` 的零拷贝与所有权模型差异。

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

待补：补上 `clone`、`appendChain`、`coalesce` 路径的 benchmark，以及与深拷贝缓冲方案的对照。

## cpplings 练习入口

- [`span1` — std::span 非拥有视图](../../../exercises/cpp20/span1.cpp)
- [`cachefriendly1` — 缓存友好的数据结构](../../../exercises/topics/cachefriendly1.cpp)
- [`perf1` — 性能优化技巧：缓存友好与 string_view](../../../exercises/topics/perf1.cpp)
