---
title: Abseil 字符串与哈希工具
topic: libraries
feature: strings-hash
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# Abseil 字符串与哈希工具

> 源码路径：`references/impl/abseil-cpp/absl/strings/`, `absl/hash/`

## absl::string_view

`absl::string_view` 是 `std::string_view` 的前身——Google 在 C++17 标准化之前就已在内部广泛使用。核心实现：指向外部字符串的 `(pointer, length)` 对，不拥有内存。

```cpp
class string_view {
  const char* data_;
  size_t size_;
  // sizeof = 16 字节（两个指针大小的成员）
};
```

**注意**：`absl::string_view` 与 `std::string_view` 的 API 几乎完全相同，但 Abseil 的版本是 Google 内部兼容的——它不提供某些标准版本中的 constexpr 方法，以保持与旧编译器的兼容性。在新项目中，应直接使用 `std::string_view`。

## absl::StrCat 与格式化拼接

`StrCat` 是 `std::format` 的前身，专门用于高效字符串拼接：

```cpp
// 传统方式——3 次临时分配
std::string result = "Hello, " + name + "! You have " + std::to_string(count) + " messages.";

// StrCat——预计算总长度，一次分配
std::string result = absl::StrCat("Hello, ", name, "! You have ", count, " messages.");
```

`StrCat` 的实现策略：

1. **预计算总长度**：遍历所有参数，累加每个参数的字符串长度
2. **单次分配**：分配 `total_length` 字节的输出缓冲区
3. **直接拷贝**：将每个参数直接 memcpy 到输出缓冲区的正确位置

这避免了 `std::string::operator+` 的多次分配和拷贝。

## absl::Hash

Abseil 提供自己的哈希框架，比 `std::hash` 更安全、更均匀：

```cpp
// std::hash 的问题：很多类型的 hash 函数质量差
// absl::Hash 保证高质量分布，支持所有基本类型和容器
template <typename T>
size_t hash = absl::Hash<T>{}(value);
```

`absl::Hash` 的设计：

1. **组合式哈希**：容器类型的哈希由元素哈希组合而成，而非简单异或
2. **域分离**：不同类型使用不同的哈希种子，避免 int(42) 和 double(42.0) 冲突
3. **SALSA20 流密码**：内部使用 SALSA20 的混合函数确保高质量扩散

```cpp
// absl::Hash 的组合函数（简化）
void H1::Combine(size_t seed, size_t value) {
  // 基于 SALSA20 quarter-round 的混合函数
  seed += value * 0x9e3779b97f4a7c15;  // 黄金比例乘数
  seed = absl::rotl(seed, 11);
  seed *= 0x243f6a8885a308d3;  // π 的小数部分
}
```

**与 std::hash 的对比**：

| 维度 | `std::hash` | `absl::Hash` |
|------|-----------|-------------|
| 质量 | 不保证（很多实现用 identity） | 保证高质量分布 |
| 安全性 | 不防碰撞攻击 | **SipHash/SALSA20 混合** |
| 组合 | 无标准方式 | `H1::Combine` |
| 容器支持 | 无 | 自动支持 vector/pair/tuple |
| 编译期 | 有限 | constexpr 友好 |
