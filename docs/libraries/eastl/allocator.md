---
title: EASTL 分配器模型
topic: libraries
feature: eastl-allocator
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# EASTL 分配器模型：非模板、实例化设计

> 源码路径：`references/impl/EASTL/include/EASTL/internal/config.h`, `allocator.h`

## 为什么需要新分配器

标准分配器 `std::allocator<T>` 的根本问题在于它是**类型绑定的、无状态的**：

```cpp
template <typename T>
class allocator {
public:
    T* allocate(size_t n);         // 调用 ::operator new
    void deallocate(T* p, size_t n);
};
```

游戏引擎需要：按子系统标记分配（渲染、物理、AI）、对齐控制（SIMD）、分配追踪、运行时切换分配策略。标准分配器类型绑定的设计使得这些很难实现。

## EASTL 分配器：非模板实例

```cpp
class EASTL_API allocator
{
public:
    EASTL_ALLOCATOR_EXPLICIT allocator(
        const char* pName = EASTL_NAME_VAL(EASTL_ALLOCATOR_DEFAULT_NAME));
    allocator(const allocator& x);
    allocator(const allocator& x, const char* pName);
    allocator& operator=(const allocator& x);

    void* allocate(size_t n, int flags = 0);
    void* allocate(size_t n, size_t alignment, size_t offset, int flags = 0);
    void  deallocate(void* p, size_t n);

    const char* get_name() const;
    void        set_name(const char* pName);

protected:
#if EASTL_NAME_ENABLED
    const char* mpName;   // 调试名称
#endif
};
```

### 关键设计差异

**1. 不绑定类型**：返回 `void*`，容器自行转换。

**2. 双重 allocate 重载**：
- `allocate(n, flags)` — 基本分配，flags 传递语义（`MEM_TEMP = 0`，`MEM_PERM = 1`）
- `allocate(n, alignment, offset, flags)` — 对齐分配

**3. 所有实例比较相等**：

```cpp
inline bool operator==(const allocator&, const allocator&)
{
    return true;  // 所有分配器被视为相等
}
```

这意味着容器 swap 时不需要检查分配器兼容性，直接交换三指针。

**4. dummy_allocator**：不执行任何操作的哑分配器，所有 `allocate()` 返回 NULL。用于 `bEnableOverflow = false` 的 fixed 容器。

## 容器使用模式

```cpp
eastl::allocator gameAlloc("GameHeap");
eastl::vector<RenderCommand> commands(gameAlloc);

eastl::allocator physAlloc("PhysicsHeap");
eastl::vector<RigidBody> bodies(physAlloc);
```

`VectorBase` 使用 `compressed_pair<T*, allocator_type>` 存储容量指针和分配器实例，在空分配器情况下节省一个指针的空间。
