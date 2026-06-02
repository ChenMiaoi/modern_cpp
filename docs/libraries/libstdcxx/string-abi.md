# libstdc++ string：从 COW 到 SSO 的 ABI 迁移

## COW 时代（GCC 4.x，ABI v1）

libstdc++ 在 C++11 之前使用写时复制。**string 对象本身只有 1 个指针**（32 位系统下 4 字节！）：

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
```

### COW 的致命问题（C++11 后不合规）

```cpp
string a = "hello";
char& c = a[0];   // 获取引用
string b = a;      // COW：b 和 a 共享数据，refcount=2
c = 'H';           // 写入触发 lazy copy → b 的数据被影响
                   // 但 c 引用的是 a[0]，COW 实现可能违反标准
```

### 全局空字符串实例

```
 ┌────────────────────────┐
 │ _M_length   = 0        │
 │ _M_capacity = 0        │
 │ _M_refcount = -1       │ ← 永不释放
 ├────────────────────────┤
 │ \0                     │
 └────────────────────────┘
 所有空 string 的 _M_p → 此全局实例
```

## SSO 迁移（GCC 5.0，ABI v2）

```
  basic_string 对象 (32 字节)
  ┌───────────────────────────────────────────────────────────────────────┐
  │ 偏移   0 ──  7  │ _M_dataplus._M_p       (指向实际字符数据)          │
  │ 偏移   8 ── 15  │ _M_dataplus._M_alloc   (空 allocator, EBO 压缩)   │
  │ 偏移  16 ── 23  │ union: _M_string_length │ _M_local_buf[16] 前 8 字节│
  │ 偏移  24 ── 31  │ _M_allocated_capacity  │ _M_local_buf[16] 后 8 字节│
  └───────────────────────────────────────────────────────────────────────┘

  Short（≤15）：_M_p 指向自身的 _M_local_buf
  Long（>15）：_M_p 指向堆分配

  SSO 容量 = 16 - 1 = 15 字节  (比 libc++ 的 22 少 7 字节)
  sizeof = 32 字节              (比 libc++ 的 24 多 8 字节)
```

## 双 ABI 共存

```
  -D_GLIBCXX_USE_CXX11_ABI=0          默认 (=1)
  ┌──────────────────────────────┐    ┌────────────────────────────┐
  │ namespace std {              │    │ namespace std {            │
  │   inline namespace __cxx11 { │    │   // (无内联命名空间)      │
  │     class basic_string;      │    │   class basic_string;      │
  │   }                          │    │ }                          │
  │ }                            │    │                            │
  └──────────────────────────────┘    └────────────────────────────┘

  ABI v1: _ZNSt12basic_stringIcSt11char_traitsIcESaIcEE...
  ABI v2: _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE...
                   ^^^^^^^  命名空间注入 + [abi:cxx11] tag
```

`abi_tag` 属性将 ABI 标签编码到符号名中，使同一 .so 中可以同时存在两种 ABI 的 string 类型。

**ABI 稳定的代价**：libstdc++ 不得不保留次优实现。例如 `std::list::size()` 必须是 O(1)（ABI 锁定），即使这使得 `splice` 需要 O(n) 的计数更新。

## 用户 API

用户侧看到的是 `std::string` 的构造、拼接、修改与 `data()/c_str()` 访问；本文现有正文主要解释这些 API 背后的 COW→SSO ABI 迁移。

## 标准语义

待补：把 C++11 之后对字符串引用稳定性、连续存储与可写字符访问的标准语义，和旧 COW 实现的冲突逐条对应起来。

## 对象布局

上文已经覆盖 COW 单指针布局、SSO 时代的 32 字节对象布局与双 ABI 符号；后续补一张 v1/v2 并排偏移图。

## 核心源码路径

待补：补上 `basic_string.h`、`basic_string.tcc`、`cow_string.h`（历史实现）等路径，串起 ABI v1/v2 的分叉点。

## 核心类 / 函数

待补：统一整理 `_M_dataplus`、`_M_local_buf`、`_M_allocated_capacity`、历史 `_Rep`/`_M_refcount` 与 ABI 选择宏。

## 关键算法

待补：补充 short/long 模式切换、扩容增长、历史 COW 分离写时复制路径与双 ABI 符号选择流程。

## ABI 约束

本篇主题本身就是 ABI；后续这里补齐 `__cxx11` 命名空间、`abi_tag`、`_GLIBCXX_USE_CXX11_ABI` 与混用二进制边界的约束清单。

## 异常安全

待补：补充 `append` / `reserve` / `replace` 在分配失败时的保证，以及 COW 时代分离副本失败时的行为差异。

## iterator / reference invalidation

待补：明确 SSO/堆扩容、`shrink_to_fit`、非 const 修改对 `data()`、iterator、reference 的失效边界，并对比旧 COW 时代的额外陷阱。

## 性能模型

待补：补上 15-byte SSO、32-byte 对象大小、双 ABI 兼容成本以及 COW 被移除后复制/写入路径的性能权衡。

## libstdc++ vs libc++ vs MSVC

正文已给出 SSO 容量和对象大小的部分差异；后续在这里补齐三家 string 布局、增长策略、调试模式与 ABI 策略对照。

## 最小复现代码

```cpp
#include <string>

int main() {
  std::string s = "hello";
  s += " world";
  return static_cast<int>(s.size());
}
```

## 编译 / 反汇编 / benchmark 证据

待补：补上短串/长串分界、`append` 热路径、双 ABI 符号名与对象大小的编译/反汇编/benchmark 证据。

## cpplings 练习入口

- [`stringview1` — std::string_view 非拥有字符串视图](../../../exercises/cpp17/stringview1.cpp)
- [`perf1` — 性能优化技巧：SBO、缓存友好、string_view](../../../exercises/topics/perf1.cpp)
