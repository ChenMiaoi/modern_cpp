# 优化与性能惯用语

## Copy Elision（拷贝消除）

编译器省略不必要的拷贝/移动构造。C++17 起 prvalue 的 copy elision 是**强制的**。

## SBO（Small Buffer Optimization）

小对象内联存储在栈上，避免堆分配：

```cpp
// std::function 的典型 SBO：24 字节栈缓冲区
// 一个捕获 2 个指针的 lambda（16B）→ SBO，无堆分配
// 一个捕获 3 个指针的 lambda（24B）→ 可能堆分配
```

## SSO（Small String Optimization）

SSO 是 SBO 的特例——字符串短时内联存储：

```
libc++:     SSO = 22 字节，sizeof(string) = 24
libstdc++:  SSO = 15 字节，sizeof(string) = 32
fbstring:   SSO = 23 字节，sizeof(string) = 24
```

## COW（Copy-On-Write，写时复制）

多个对象共享同一块数据，只在写入时才复制。C++11 后因线程安全问题被标准库弃用——`std::string` 的 COW 实现在 C++11 中不合规。

## Devirtualization（去虚化）

编译器在能够确定对象的动态类型时，将虚函数调用替换为直接调用：

```cpp
Derived d;
Base& b = d;
b.virtual_func();  // 编译器可能直接调用 Derived::virtual_func()
```

GCC/Clang 通过 LTO 和 `-fdevirtualize` 自动进行去虚化。

## Branch Prediction（分支预测）

CPU 在条件分支结果确定之前猜测走哪个方向：

```cpp
// [[likely]] / [[unlikely]] (C++20) 帮助编译器优化分支
if (condition) [[unlikely]] {
  error_handling();
} else [[likely]] {
  normal_path();
}
```

## Cache-Friendly Design

```
  容器选择与缓存行为：

  std::vector  → 连续内存 → 遍历极快（预取友好）
  std::list    → 节点分散 → 遍历慢（cache miss）
  flat_map     → 排序数组 → 查找快（二分 + 连续内存）
  std::map     → 红黑树   → 节点跳跃（cache miss）

  经验法则：
  - 小容器（<1000 元素）→ vector + sort + binary_search 几乎总是最快
  - 中等容器 → flat_map/flat_set
  - 大容器 + 频繁插入删除 → unordered_map 或 map
```

## Inline

编译器将函数调用替换为函数体——消除调用开销、启用更多优化：

```cpp
inline int square(int x) { return x * x; }
// 建议但不强制——编译器有自己的内联启发式

[[gnu::always_inline]] void hot_path();  // 强制内联（GCC/Clang）
__declspec(noinline) void cold_path();   // 禁止内联（MSVC）
```

## Prefetch（预取）

告诉 CPU 提前将数据加载到缓存：

```cpp
// GCC/Clang 内建
__builtin_prefetch(&data[i + 64]);  // 预取未来会访问的数据
```

## LTO（Link-Time Optimization）

链接时优化——允许跨翻译单元的内联、死代码消除、常量传播。ThinLTO（LLVM）支持并行链接时优化，是生产环境的推荐选择。
