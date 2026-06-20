---
title: "排序算法实现分析"
topic: internals
feature: sorting
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_algo.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__algorithm/sort.h"
---

# 排序算法实现分析

> `std::sort` 是 C++ 中最常用的排序算法，GCC 和 LLVM 都使用 IntroSort 作为默认实现。本文基于源码分析排序算法的内部实现。

---

## 一、IntroSort 算法

### 1.1 什么是 IntroSort

IntroSort（introspective sort）是一种混合排序算法，结合了快速排序、堆排序和插入排序的优点：

```
IntroSort 的策略：

1. 快速排序（主要）：
   · 平均 O(n log n)
   · 缓存友好
   · 最坏情况 O(n²)

2. 堆排序（回退）：
   · 当递归深度超过限制时使用
   · 保证 O(n log n)
   · 但缓存不友好

3. 插入排序（小数组）：
   · 当数组小于阈值时使用
   · 对小数组效率更高
   · 缓存友好
```

### 1.2 IntroSort 的流程

```
IntroSort 的执行流程：

1. 检查数组大小
   · 如果 size < 阈值（通常 16）→ 使用插入排序
   · 否则继续

2. 快速排序分区
   · 选择 pivot（通常使用三数取中法）
   · 分区数组
   · 递归排序两个子数组

3. 检查递归深度
   · 如果深度超过限制（通常 2 × log2(n)）→ 切换到堆排序

4. 最终插入排序
   · 快速排序后，数组基本有序
   · 使用插入排序完成最终排序
```

---

## 二、GCC (libstdc++) 的实现

### 2.1 主排序函数（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_algo.h:1908

template<typename _RandomAccessIterator, typename _Compare>
inline void
__sort(_RandomAccessIterator __first, _RandomAccessIterator __last,
       _Compare __comp) {
    if (__first != __last) {
        // 1. IntroSort 主循环
        // 深度限制 = 2 × log2(n)
        std::__introsort_loop(__first, __last,
                              std::__lg(__last - __first) * 2,  // 深度限制
                              __comp);
        // 2. 最终插入排序
        std::__final_insertion_sort(__first, __last, __comp);
    }
}

// __lg 计算 log2(n)
template<typename _Tp>
constexpr _Tp __lg(_Tp __n) noexcept {
    _Tp __k = 0;
    while (__n > 1) {
        __n >>= 1;
        ++__k;
    }
    return __k;
}
```

### 2.2 IntroSort 循环（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_algo.h:1884

template<typename _RandomAccessIterator, typename _Size, typename _Compare>
void
__introsort_loop(_RandomAccessIterator __first,
                 _RandomAccessIterator __last,
                 _Size __depth_limit, _Compare __comp) {
    while (__last - __first > int(_S_threshold)) {  // _S_threshold = 16
        if (__depth_limit == 0) {
            // 深度超限，切换到堆排序（O(n log n) 保证）
            std::__partial_sort(__first, __last, __last, __comp);
            return;
        }
        --__depth_limit;
        
        // 快速排序分区：选择 pivot 并分区
        _RandomAccessIterator __cut =
            std::__unguarded_partition_pivot(__first, __last, __comp);
        
        // 递归排序右半部分（尾递归优化：只递归较大的一半）
        std::__introsort_loop(__cut, __last, __depth_limit, __comp);
        __last = __cut;  // 尾递归：迭代处理较小的一半
    }
}
```

### 2.3 分区算法（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_algo.h:1833

// 无保护分区（不检查边界）
template<typename _RandomAccessIterator, typename _Compare>
_RandomAccessIterator
__unguarded_partition(_RandomAccessIterator __first,
                      _RandomAccessIterator __last,
                      _RandomAccessIterator __pivot, _Compare __comp) {
    while (true) {
        // 从左到右找到第一个 >= pivot 的元素
        while (__comp(*__first, *__pivot))
            ++__first;
        
        // 从右到左找到第一个 <= pivot 的元素
        --__last;
        while (__comp(*__pivot, *__last))
            --__last;
        
        // 如果指针相遇或交叉，返回分区点
        if (!(__first < __last))
            return __first;
        
        // 交换两个元素
        std::iter_swap(__first, __last);
        ++__first;
    }
}

// 选择 pivot（三数取中法）
template<typename _RandomAccessIterator, typename _Compare>
inline _RandomAccessIterator
__unguarded_partition_pivot(_RandomAccessIterator __first,
                            _RandomAccessIterator __last, _Compare __comp) {
    typedef iterator_traits<_RandomAccessIterator> _IterTraits;
    typedef typename _IterTraits::difference_type _Dist;
    
    // 取中间元素
    _RandomAccessIterator __mid = __first + _Dist((__last - __first) / 2);
    _RandomAccessIterator __second = __first + _Dist(1);
    
    // 三数取中：将中位数移到第一个位置
    std::__move_median_to_first(__first, __second, __mid, __last - _Dist(1),
                                __comp);
    
    // 分区
    return std::__unguarded_partition(__second, __last, __first, __comp);
}
```

### 2.4 最终插入排序（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_algo.h:1813

template<typename _RandomAccessIterator, typename _Compare>
void
__final_insertion_sort(_RandomAccessIterator __first,
                       _RandomAccessIterator __last, _Compare __comp) {
    typename iterator_traits<_RandomAccessIterator>::difference_type
        __threshold = _S_threshold;  // 16
    
    if (__last - __first > __threshold) {
        // 数组较大：前半部分用插入排序，后半部分用未保护插入排序
        std::__insertion_sort(__first, __first + __threshold, __comp);
        std::__unguarded_insertion_sort(__first + __threshold, __last,
                                        __comp);
    } else {
        // 数组较小：直接用插入排序
        std::__insertion_sort(__first, __last, __comp);
    }
}
```

---

## 三、LLVM (libc++) 的实现

### 3.1 sort 函数

LLVM 也使用类似的 IntroSort 实现：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__algorithm/sort.h

template <class _RandomAccessIterator, class _Compare>
void sort(_RandomAccessIterator __first, _RandomAccessIterator __last, _Compare __comp) {
    // IntroSort 实现
    // 1. 检查大小
    // 2. 快速排序分区
    // 3. 深度限制检查
    // 4. 最终插入排序
}
```

### 3.2 小数组优化

LLVM 对小数组有特殊优化：

```
LLVM 的小数组优化：

· size ≤ 16：直接使用插入排序
· size ≤ __max_larger_than_qsort：使用快速排序
· 其他：使用 IntroSort

__max_larger_than_qsort 通常是 16-32
```

---

## 四、分区算法

### 4.1 三数取中法

```cpp
// 选择 pivot 的策略
template<typename _Iterator, typename _Compare>
void
__move_median_to_first(_Iterator __result, _Iterator __a, _Iterator __b,
                       _Iterator __c, _Compare __comp) {
    if (__comp(*__a, *__b)) {
        if (__comp(*__b, *__c))
            std::iter_swap(__result, __b);      // a < b < c
        else if (__comp(*__a, *__c))
            std::iter_swap(__result, __c);      // a < c < b
        else
            std::iter_swap(__result, __a);      // c < a < b
    } else if (__comp(*__a, *__c))
        std::iter_swap(__result, __a);          // b < a < c
    else if (__comp(*__b, *__c))
        std::iter_swap(__result, __c);          // b < c < a
    else
        std::iter_swap(__result, __b);          // c < b < a
}
```

### 4.2 分区算法

```
分区算法的步骤：

1. 选择 pivot（三数取中法）
2. 将 pivot 移动到第一个位置
3. 从左到右扫描，找到大于 pivot 的元素
4. 从右到左扫描，找到小于 pivot 的元素
5. 交换这两个元素
6. 重复 3-5 直到指针相遇
7. 将 pivot 放到正确位置
```

---

## 五、stable_sort 的实现

### 5.1 TimSort

GCC 和 LLVM 的 `std::stable_sort` 通常使用 TimSort：

```
TimSort 的特点：

1. 归并排序的变体
2. 对已部分排序的数据效率很高
3. 保证 O(n log n) 时间复杂度
4. 保证稳定排序

TimSort 的策略：

1. 识别已排序的 runs（升序或降序）
2. 合并相邻的 runs
3. 使用插入排序扩展小 runs
```

---

## 六、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ sort 算法              │ IntroSort            │ IntroSort            │
│ stable_sort 算法       │ 归并排序             │ TimSort              │
│ 小数组阈值             │ 16                   │ 16-32                │
│ 深度限制               │ 2 × log2(n)         │ 2 × log2(n)         │
│ pivot 选择             │ 三数取中法           │ 三数取中法           │
│ constexpr 支持         │ C++20                │ C++20                │
│ ranges::sort           │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 七、性能特征

```
排序算法的性能：

sort：
  · 平均：O(n log n)
  · 最坏：O(n log n)（IntroSort 保证）
  · 空间：O(log n)（递归栈）

stable_sort：
  · 平均：O(n log n)
  · 最坏：O(n log n)
  · 空间：O(n)（需要额外空间）

与 std::qsort 对比：
  · sort：O(n log n)，内联，无函数指针开销
  · qsort：O(n log n)，函数指针开销，无法内联
  · sort 比 qsort 快 2-5 倍
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — vector 的排序优化
- [std::map/set 实现](/internals/containers/map-set) — 红黑树的有序存储
- [Ranges 框架](/internals/algorithms/ranges) — ranges::sort 的实现
