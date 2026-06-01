# Abseil Cord：面向大文本的 B-tree 缓冲区

> 源码路径：`references/impl/abseil-cpp/absl/strings/cord.h`, `cord_internal.h`

`absl::Cord` 是 Google 为**大文本拼接**场景设计的字符串类型。与 `std::string` 不同，`Cord` 不要求字符数据连续存储——它使用 B-tree 结构将文本分成多个 chunk，拼接操作仅修改树结构，不复制数据。

## 核心问题：大字符串拼接

`std::string` 在反复 `append` 时的性能问题：

```
string s;
s += chunk1;  // 分配 2x 容量，拷贝
s += chunk2;  // 可能触发扩容，拷贝全部
s += chunk3;  // 同上
...
s += chunkN;  // O(N²) 总拷贝量
```

对于日志聚合、protobuf 序列化、HTTP body 拼接等场景（频繁拼接大块文本），`std::string` 的 O(n²) 复杂度不可接受。

## Cord 的 B-tree 结构

```
  Cord 内部树结构示意:

       ┌────────────────────┐
       │  CordRep Concat    │    根节点
       │  (left, right)     │
       └─────────┬──────────┘
          ┌──────┴──────┐
          ▼             ▼
  ┌──────────────┐  ┌──────────────┐
  │ CordRep Leaf │  │ CordRep Concat│
  │ "Hello, "    │  │ (left, right) │
  └──────────────┘  └───────┬──────┘
                      ┌─────┴─────┐
                      ▼           ▼
              ┌──────────────┐ ┌──────────────┐
              │ CordRep Leaf │ │ CordRep External│
              │ "World"      │ │ (ref to mmap)   │
              └──────────────┘ └──────────────┘

  CordRep 类型:
  - Leaf:       内联小数据（最多 12 字节在 Rep 内部）
  - External:   指向外部内存（mmap、protobuf buffer 等）
  - Concat:     二叉拼接节点（left + right）
  - Substring:  引用另一个 Rep 的子区间（零拷贝切片）
  - Rope:       多子节点的平衡树（自动从 Concat 转换）
```

## CordRep 核心设计

```cpp
struct CordRep {
  // 引用计数（atomic）
  std::atomic<size_t> refcount;
  
  // 数据长度
  size_t length;
  
  // 类型标签
  enum Type { EXTERNAL, CONCAT, SUBSTRING, LEAF, ROPE };
  Type tag;
  
  union {
    struct { char data[12]; } leaf;          // 内联小数据
    struct { void* data; FreeFunc free; } ext;  // 外部存储
    struct { CordRep* left; CordRep* right; } concat;  // 拼接
    struct { CordRep* child; size_t offset; size_t length; } sub;  // 子串
  };
};
```

**关键设计**：Cord 的拼接操作是 O(1)——只需创建一个 Concat 节点，指向左右两个子树。没有数据拷贝。

## 零拷贝拼接

```cpp
absl::Cord a("Hello, ");
absl::Cord b("World!");
absl::Cord c = a + b;  // O(1): 创建 Concat 节点，a 和 b 不变
```

拼接操作：
1. 创建新的 `CordRep Concat` 节点
2. `left = a.tree()`，`right = b.tree()`
3. `refcount = 1`，`length = a.length() + b.length()`
4. 不复制任何字符数据

## Flat 表示（小字符串优化）

当 Cord 的数据量很小时，退化为平坦表示：

```cpp
struct CordRepInline {
  char data[kMaxInline];  // 通常 12-15 字节
  size_t length;
};
```

小于 `kMaxInline` 的 Cord 完全不分配堆内存。

## 外部存储引用

Cord 可以引用外部内存，零拷贝：

```cpp
// 引用 mmap 的文件内容——不拷贝
absl::Cord FromMmap(const void* data, size_t len) {
  return absl::MakeCordFromExternal(
      absl::string_view(static_cast<const char*>(data), len),
      [data, len](absl::string_view) { munmap(const_cast<void*>(data), len); });
}
```

## 与 std::string 的对比

| 维度 | `std::string` | `absl::Cord` |
|------|--------------|-------------|
| 存储 | 连续内存 | B-tree（多 chunk） |
| 拼接 | O(n²) 累计 | **O(1) 每次** |
| 随机访问 | O(1) | O(log n) |
| 子串 | O(n) 拷贝 | **O(1) 零拷贝** |
| 外部引用 | 不支持 | **支持** |
| SSO | 15-22 字节 | 12-15 字节 |
| 适用场景 | 通用小字符串 | 大文本、日志聚合、protobuf |
