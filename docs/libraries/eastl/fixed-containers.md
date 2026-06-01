# EASTL fixed_* 容器：零堆分配的确定性容器

> 源码路径：`references/impl/EASTL/include/EASTL/fixed_vector.h`, `fixed_string.h`

## fixed_vector 内存布局

```
fixed_vector<T, nodeCount, bEnableOverflow> 对象内存布局:

+----------------------------------------------------------+
| fixed_vector 对象 (sizeof 包含整个内嵌缓冲区)              |
|                                                          |
|  VectorBase<T, fixed_allocator>                          |
|    T* mpBegin       ─────────────────────────┐           |
|    T* mpEnd         ─────────────────────┐   │           |
|    T* capacityPtr   (= mpBegin + N)      │   │           |
|                                           |   |           |
|  aligned_buffer<N * sizeof(T), alignof(T)>|   |           |
|    +-------+-------+-------+-----+--------+              |
|    | T[0]  | T[1]  | T[2]  | ... | T[N-1] |             |
|    +-------+-------+-------+-----+--------+              |
|    ^mpBegin  ^mpEnd (初始时两者相等)                       |
+----------------------------------------------------------+

push_back() 路径决策:

            size() < nodeCount ?
                  |
           +------+------+
           |             |
         [是]          [否]
           |             |
           v             v
 写入 mBuffer      bEnableOverflow == true ?
 (内嵌缓冲区)            |
     O(1)        +------+------+
                 |             |
               [true]       [false]
                 |             |
                 v             v
        OverflowAllocator   dummy_allocator
        (默认 EASTLAlloc)   (返回 NULL)
        堆上分配溢出空间    -> 未定义行为

两种典型用法:
  fixed_vector<int, 64, false> v;   // 纯栈模式, 超过 64 -> assert/UB
  fixed_vector<int, 64, true>  v;   // 混合模式, 前 64 栈内, 之后堆
```

## 模板参数

| 参数 | 语义 |
|------|------|
| `T` | 元素类型 |
| `nodeCount` | 内嵌存储容量 |
| `bEnableOverflow` | 容量耗尽时是否回退到堆分配 |
| `OverflowAllocator` | 溢出时使用的分配器类型 |

## 构造过程

```cpp
fixed_vector()
    : base_type(fixed_allocator_type(mBuffer.buffer))
{
    mpBegin = mpEnd = (value_type*)&mBuffer.buffer[0];
    internalCapacityPtr() = mpBegin + nodeCount;
}
```

`mpBegin` 指向 `mBuffer.buffer`——这是栈上的内存，不是堆。容量在构造时已固定。

## 移动构造的特殊性

移动构造**不能做指针交换**（内嵌缓冲区是对象的一部分），必须逐元素搬移到自身的 `mBuffer` 中。这比普通的 vector 移动构造更慢。
