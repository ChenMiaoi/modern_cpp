---
title: "C++ 内部原理 - 文章索引"
topic: internals
feature: index
standard: N/A
status_checked_at: 2026-06-20
---
# C++ 内部原理 - 完整文章索引

> 本栏目基于 GCC (libstdc++) 和 LLVM (libc++) 的源码，深入分析 C++ 语言和标准库的底层实现机制。共 **63 篇**文章，覆盖 C++ 的方方面面。

---

## 一、运行时与 ABI（6 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 1 | [对象模型与内存布局](/internals/runtime/object-model) | `runtime/object-model.md` | vptr 位置、EBO、Itanium ABI 布局规则 |
| 2 | [虚函数表实现](/internals/runtime/vtable) | `runtime/vtable.md` | vtable 结构、thunk 机制、VTT、虚析构双槽位 |
| 3 | [RTTI 与 dynamic_cast](/internals/runtime/rtti) | `runtime/rtti.md` | type_info、dynamic_cast 算法、-fno-rtti |
| 4 | [异常处理 ABI](/internals/runtime/exception-handling) | `runtime/exception-handling.md` | .eh_frame、LSDA、栈展开、__cxa_throw |
| 5 | [名称修饰](/internals/runtime/name-mangling) | `runtime/name-mangling.md` | Itanium ABI 编码、MSVC 编码、c++filt |
| 6 | [调用约定](/internals/runtime/calling-conventions) | `runtime/calling-conventions.md` | SysV AMD64、MSVC x64、结构体传递 |

---

## 二、容器实现（11 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 7 | [std::vector](/internals/containers/vector) | `containers/vector.md` | 三指针布局、扩容策略、vector\<bool\> 特化 |
| 8 | [std::string](/internals/containers/string) | `containers/string.md` | SSO、COW、双 ABI、_M_mutate |
| 9 | [std::list](/internals/containers/list) | `containers/list.md` | 双向链表、哨兵节点、splice |
| 10 | [std::forward_list](/internals/containers/forward-list) | `containers/forward-list.md` | 单向链表、before_begin、insert_after |
| 11 | [std::deque](/internals/containers/deque) | `containers/deque.md` | 中控器、缓冲区、迭代器设计 |
| 12 | [std::map/set](/internals/containers/map-set) | `containers/map-set.md` | 红黑树、旋转算法、迭代器递增 |
| 13 | [std::unordered_map](/internals/containers/unordered-map) | `containers/unordered-map.md` | 哈希表、链地址法、rehash 策略 |
| 14 | [std::array](/internals/containers/array) | `containers/array.md` | 固定大小数组、constexpr 支持 |
| 15 | [std::stack/queue](/internals/containers/stack-queue) | `containers/stack-queue.md` | 容器适配器、底层容器选择 |
| 16 | [std::flat_map](/internals/containers/flat-map) | `containers/flat-map.md` | 排序 vector、二分查找、C++23 |
| 17 | [std::inplace_vector](/internals/containers/inplace-vector) | `containers/inplace-vector.md` | 栈上固定容量、C++26 |

---

## 三、智能指针与内存（7 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 18 | [std::unique_ptr](/internals/memory/unique-ptr) | `memory/unique-ptr.md` | 压缩指针、删除器存储、移动语义控制 |
| 19 | [std::shared_ptr](/internals/memory/shared-ptr) | `memory/shared-ptr.md` | 控制块、原子引用计数、双字优化 |
| 20 | [std::weak_ptr](/internals/memory/weak-ptr) | `memory/weak-ptr.md` | 弱引用计数、lock() 机制、内存序 |
| 21 | [分配器模型](/internals/memory/allocator) | `memory/allocator.md` | std::allocator、allocator_traits、EBO |
| 22 | [PMR 多态内存资源](/internals/memory/pmr) | `memory/pmr.md` | memory_resource、polymorphic_allocator |
| 23 | [GNU 扩展分配器](/internals/memory/ext-allocators) | `memory/ext-allocators.md` | pool/bitmap/mt/malloc 分配器 |
| 24 | [内存操作基础设施](/internals/memory/operations) | `memory/operations.md` | uninitialized_copy/fill、construct_at |

---

## 四、工具类实现（7 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 25 | [std::function](/internals/utilities/function) | `utilities/function.md` | 类型擦除、SBO、管理器操作 |
| 26 | [std::variant](/internals/utilities/variant) | `utilities/variant.md` | 标签联合、visit、jump table |
| 27 | [std::any](/internals/utilities/any) | `utilities/any.md` | 类型擦除、小/大对象策略 |
| 28 | [std::optional](/internals/utilities/optional) | `utilities/optional.md` | 存储布局、constexpr、value_or |
| 29 | [std::tuple](/internals/utilities/tuple) | `utilities/tuple.md` | 递归继承、get、tuple_size |
| 30 | [std::pair](/internals/utilities/pair) | `utilities/pair.md` | EBO、piecewise_construct、三向比较 |
| 31 | [std::bitset](/internals/utilities/bitset) | `utilities/bitset.md` | 位运算、popcount、to_string |

---

## 五、类型系统与元编程（5 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 32 | [Type Traits](/internals/templates/type-traits) | `templates/type-traits.md` | integral_constant、enable_if、void_t |
| 33 | [SFINAE](/internals/templates/sfinae) | `templates/sfinae.md` | SFINAE 上下文、检测惯用法 |
| 34 | [Concepts](/internals/templates/concepts) | `templates/concepts.md` | requires 表达式、subsumption |
| 35 | [constexpr](/internals/templates/constexpr) | `templates/constexpr.md` | 编译期求值、consteval、constinit |
| 36 | [模板实例化](/internals/templates/instantiation) | `templates/instantiation.md` | 两阶段查找、显式实例化、偏特化 |

---

## 六、算法与迭代器（4 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 37 | [迭代器体系](/internals/algorithms/iterators) | `algorithms/iterators.md` | iterator_traits、迭代器标签、指针特化 |
| 38 | [排序算法](/internals/algorithms/sorting) | `algorithms/sorting.md` | IntroSort、三数取中、分区算法 |
| 39 | [Ranges](/internals/algorithms/ranges) | `algorithms/ranges.md` | view_interface、管道运算符、惰性求值 |
| 40 | [并行算法](/internals/algorithms/parallel) | `algorithms/parallel.md` | 执行策略、TBB/OpenMP 后端、SIMD |

---

## 七、并发原语（6 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 41 | [std::atomic](/internals/concurrency/atomic) | `concurrency/atomic.md` | 原子操作内建函数、memory_order、atomic_flag |
| 42 | [std::thread](/internals/concurrency/thread) | `concurrency/thread.md` | pthread 封装、join/detach、this_thread |
| 43 | [std::mutex](/internals/concurrency/mutex) | `concurrency/mutex.md` | lock_guard、unique_lock、RAII |
| 44 | [std::condition_variable](/internals/concurrency/condition-variable) | `concurrency/condition-variable.md` | wait/notify、虚假唤醒、cv_status |
| 45 | [std::future/promise](/internals/concurrency/future) | `concurrency/future.md` | shared_state、原子操作、条件变量 |
| 46 | [std::jthread](/internals/concurrency/jthread) | `concurrency/jthread.md` | stop_token、stop_callback、协作式取消 |

---

## 八、I/O 与格式化（3 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 47 | [std::format](/internals/io/format) | `io/format.md` | 编译期解析、formatter、format_context |
| 48 | [std::print](/internals/io/print) | `io/print.md` | 缓冲输出、FILE*、fwrite_unlocked |
| 49 | [流类体系](/internals/io/streams) | `io/streams.md` | basic_ostream、streambuf、缓冲区管理 |

---

## 九、C++20/23 新特性（6 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 50 | [协程 Lowering](/internals/cpp20/coroutines) | `cpp20/coroutines.md` | 协程帧、promise_type、suspend_always |
| 51 | [Modules](/internals/cpp20/modules) | `cpp20/modules.md` | BMI、模块分区、头单元 |
| 52 | [std::span](/internals/cpp20/span) | `cpp20/span.md` | 非拥有视图、dynamic_extent、subspan |
| 53 | [std::expected](/internals/cpp23/expected) | `cpp23/expected.md` | unexpected、transform、and_then |
| 54 | [std::generator](/internals/cpp23/generator) | `cpp23/generator.md` | co_yield、惰性求值、迭代器 |
| 55 | [std::mdspan](/internals/cpp23/mdspan) | `cpp23/mdspan.md` | extents、layout_right/left、多维访问 |

---

## 十、编译器内部（4 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 56 | [GCC 编译器内部](/internals/compiler/gcc-internals) | `compiler/gcc-internals.md` | GENERIC/GIMPLE/RTL、C++ 前端 |
| 57 | [Clang/LLVM 编译器内部](/internals/compiler/clang-internals) | `compiler/clang-internals.md` | Clang AST、LLVM IR、代码生成 |
| 58 | [去虚拟化](/internals/compiler/devirtualization) | `compiler/devirtualization.md` | 类型传播、LTO、final 优化 |
| 59 | [编译器优化管线](/internals/compiler/optimization-pipeline) | `compiler/optimization-pipeline.md` | 内联、循环优化、向量化 |

---

## 十一、标准库综合对比（4 篇）

| # | 文章 | 路径 | 核心内容 |
|---|------|------|----------|
| 60 | [容器对比](/internals/comparison/containers) | `comparison/containers.md` | vector/string/map 等布局与实现差异 |
| 61 | [智能指针对比](/internals/comparison/smart-pointers) | `comparison/smart-pointers.md` | 控制块、make_shared、ABI 兼容性 |
| 62 | [字符串 ABI 对比](/internals/comparison/string-abi) | `comparison/string-abi.md` | __cxx11 双 ABI、__1 内联命名空间 |
| 63 | [并发实现对比](/internals/comparison/concurrency) | `comparison/concurrency.md` | atomic/mutex/thread 实现差异 |

---

## 统计

| 分类 | 篇数 | 已增强源码分析 |
|------|------|----------------|
| 运行时与 ABI | 6 | 6/6 ✅ |
| 容器实现 | 11 | 11/11 ✅ |
| 智能指针与内存 | 7 | 7/7 ✅ |
| 工具类实现 | 7 | 7/7 ✅ |
| 类型系统与元编程 | 5 | 5/5 ✅ |
| 算法与迭代器 | 4 | 4/4 ✅ |
| 并发原语 | 6 | 6/6 ✅ |
| I/O 与格式化 | 3 | 3/3 ✅ |
| C++20/23 新特性 | 6 | 6/6 ✅ |
| 编译器内部 | 4 | 4/4 ✅ |
| 标准库综合对比 | 4 | 4/4 ✅ |
| **合计** | **63** | **63/63** ✅ |

---

## 源码参考

| 组件 | GCC (libstdc++) | LLVM (libc++) |
|------|-----------------|---------------|
| vector | `bits/stl_vector.h`, `bits/vector.tcc` | `__vector/vector.h` |
| string | `bits/basic_string.h`, `bits/basic_string.tcc` | `string` |
| shared_ptr | `bits/shared_ptr_base.h` | `__memory/shared_ptr.h` |
| unique_ptr | `bits/unique_ptr.h` | `__memory/unique_ptr.h` |
| allocator | `bits/allocator.h`, `bits/alloc_traits.h` | `__memory/allocator.h` |
| map/set | `bits/stl_tree.h` | `__tree` |
| unordered_map | `bits/hashtable.h` | `__hash_table` |
| atomic | `bits/atomic_base.h` | `__atomic/atomic.h` |
| mutex | `bits/std_mutex.h` | `__mutex/mutex.h` |
| thread | `bits/std_thread.h` | `__thread/thread.h` |
| format | `bits/formatfwd.h` | `__format/` |
| ranges | `bits/ranges_base.h` | `__ranges/` |
| coroutines | `coroutine` | `__coroutine/` |
