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
