# libc++ Ranges 与 unique_ptr

## Ranges：管道运算符

详细实现见 [range-v3 管道运算符章节](/libraries/range-v3/pipe-operator)。

libc++ 的 Ranges 实现直接从 range-v3 演化而来，核心机制完全相同：
- CRTP 基类 `__range_adaptor_closure<T>` 做标签分发
- `__pipeable<Fn>` 包装函数对象
- 两个 `operator|` 重载：range|closure 和 closure|closure

## std::unique_ptr：compressed_pair

```
sizeof(unique_ptr<T, default_delete<T>>) = 8 字节（64 位）

  _LIBCPP_COMPRESSED_PAIR 展开：
  ┌──────────────────────────────────┐
  │ [[no_unique_address]] T* __ptr_  │  ← 8 字节
  │ [[no_unique_address]] default_   │  ← 0 字节（空类型）
  │            delete<T> __deleter_  │
  └──────────────────────────────────┘
  sizeof = 8
```

**trivial_abi**：libc++ 对 `unique_ptr` 标记 `[[clang::trivial_abi]]`，使其可以通过寄存器传递（而非栈上的隐式指针），减少函数调用开销。GCC 和 MSVC 不支持此属性。
