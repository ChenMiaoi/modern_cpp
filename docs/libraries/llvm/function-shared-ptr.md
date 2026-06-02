---
title: libc++ function 与 shared_ptr
topic: libraries
feature: function-shared-ptr
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/__functional/function.h
      - references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h
    symbols:
      - std::function
      - __value_func
      - __policy_func
      - std::shared_ptr
      - __shared_ptr_emplace
exercises: []
solutions: []
---
# libc++ function 与 shared_ptr

## std::function：24 字节 SBO

### 数据结构

```
__value_func:
  __buf_ (24 字节栈缓冲区)     __f_ (指针)
  ┌─────────────────────────┐  ┌──────┐
  │ callable 对象 或 未使用  │──→│ 指向 │
  └─────────────────────────┘  └──────┘
                               ↑ 可能指向 __buf_（栈）或 堆
```

### SBO 判断

```
  sizeof(_Fun) ≤ 24 AND is_nothrow_copy_constructible ?
    YES → ::new(&__buf_) _Fun(move(lambda))  // 栈上构造
    NO  → new _Fun(lambda)                    // 堆分配
```

### 移动构造的特殊性

```
  src 在栈上？→ 不能偷指针，必须 clone（与拷贝相同）
  src 在堆上？→ 直接偷指针，src.__f_ = nullptr

  关键洞察：即使是 std::function 的移动构造，栈上 callable 也需要拷贝！
```

## std::shared_ptr：控制块布局

### make_shared 单次分配

```
make_shared<int>(42) 的内存布局：

  ┌──────────────────────────────────────┐
  │     __shared_ptr_emplace 控制块       │
  │  vtable ptr (8B)                      │
  │  use_count  (atomic, 4B) = 2          │
  │  weak_count (atomic, 4B) = 1          │
  │  int __elem_ = 42 (4B)               │  ← 紧跟在后面
  └──────────────────────────────────────┘
           ↑ 一次 malloc，整块连续内存

  shared_ptr<int> sp:
  ┌──────────┬──────────┐
  │ _M_ptr ──┼──────────┼──→ __elem_ (42)
  │ _M_ctrl──┼──────────┼──→ 控制块起始
  └──────────┴──────────┘
  sizeof(shared_ptr) = 16 字节（两个指针）
```

### 两级析构

```
use_count → 0: __on_zero_shared()
  → 只析构元素，不释放内存（weak_ptr 还在）

weak_count → 0: __on_zero_shared_weak()
  → 释放控制块 + 元素的整块内存
```


## 标准语义

### `std::function`

- **可调用包装器**：`std::function<Sig>` 是通用多态函数包装器，可以存储、复制和调用任何可调用目标（lambda、函数指针、`std::bind` 表达式、函数对象）——只要其签名与 `Sig` 兼容
- **SBO（Small Buffer Optimization）**：标准不保证 SBO，但所有主流实现都提供。libc++ 的阈值是 24 字节（`3 × sizeof(void*)`），且要求被包装类型满足 `is_nothrow_copy_constructible`
- **`operator()` 语义**：对空 `function` 调用 `operator()` 抛出 `bad_function_call`（§[func.wrap.badcall]）；不检查参数有效性
- **target 访问**：`target<T*>()` 在 RTTI 启用时返回指向内部存储的 `T*`；`target_type()` 返回 `type_info`
- **Allocator 扩展**（C++11–C++14，已弃用）：`function(allocator_arg, alloc, f)` 允许自定义存储分配器；libc++ 实现中此参数被忽略，仅通过 `__value_func` 的 SBO/堆路径处理

### `std::shared_ptr`

- **共享所有权**：多个 `shared_ptr` 可以共享同一对象的所有权；最后一个被销毁的 `shared_ptr` 负责删除对象（§[util.smartptr.shared.general]）
- **控制块**：每个 `shared_ptr` 内部持有 `(element_ptr, control_block_ptr)` 两个指针（16 字节）。控制块维护 `use_count` 和 `weak_count`，通过虚函数分发析构逻辑
- **make_shared 语义**：`make_shared<T>(args)` 保证单一内存分配，将控制块与元素合并为连续内存；`shared_ptr<T>(new T(...))` 至少两次分配（元素 + 控制块）
- **weak_ptr 与锁**：`weak_ptr` 不影响 `use_count`；`weak_ptr::lock()` 原子地检查 `use_count > 0` 并创建新的 `shared_ptr`，失败时返回空 `shared_ptr`
- **enable_shared_from_this**：当类型继承 `enable_shared_from_this<T>` 且使用 `shared_ptr(new T(...))` 或 `make_shared` 构造时，自动设置内部 `weak_ptr` 使 `shared_from_this()` 可用
- **线程安全**：标准保证控制块的引用计数操作（`__add_shared`、`__release_shared`、`__add_weak`、`__release_weak`）是原子的；`shared_ptr` 对象本身的拷贝/析构需要外部同步（§[util.smartptr.shared.general]）

## 核心源码路径

### std::function

| 文件 | 内容 |
|------|------|
| `__functional/function.h` | `__base`（虚接口，含 `__clone`、`destroy`、`destroy_deallocate`、`operator()`、`target`、`target_type`）；`__func`（模板实现，存 `__func_` 成员）；`__value_func`（3×sizeof(void*) 的 SBO 缓冲 + `__f_` 指针）；`__policy_func`（`__policy_storage` union + 函数指针表）；`function` 主类定义 |
| `__type_traits/invoke.h` | `__invoke_r<_Rp>`：统一的可调用调用入口，处理函数指针、成员指针、函数对象 |

### std::shared_ptr

| 文件 | 内容 |
|------|------|
| `__memory/shared_count.h` | `__shared_count`（持有 `__shared_owners_`、`__add_shared`、`__release_shared`、纯虚 `__on_zero_shared`）；`__shared_weak_count`（增加 `__shared_weak_owners_`、`__add_weak`、`__release_weak`、纯虚 `__on_zero_shared_weak`）；`__libcpp_atomic_refcount_increment`（`__ATOMIC_RELAXED`）；`__libcpp_atomic_refcount_decrement`（`__ATOMIC_ACQ_REL`） |
| `__memory/shared_ptr.h` | `__shared_ptr_pointer`（`shared_ptr(new T(...))` 控制块，用 `_LIBCPP_COMPRESSED_TRIPLE` 存 `(ptr, deleter, alloc)`）；`__shared_ptr_emplace`（`make_shared` 控制块，`_Storage` 内联 `(alloc, elem)` 通过 `_LIBCPP_COMPRESSED_PAIR` 压缩）；`shared_ptr` 类定义（两个指针：`__ptr_`、`__cntrl_`） |
| `__memory/compressed_pair.h` | `_LIBCPP_COMPRESSED_PAIR`、`_LIBCPP_COMPRESSED_TRIPLE` 宏：空类 EBO + `[[no_unique_address]]` 消除尾部填充 |

## 核心类 / 函数

### function 侧

| 类型 / 函数 | 源码位置 | 说明 |
|-------------|----------|------|
| `__base<Sig>` | `function.h:134` | 抽象接口，7 个虚函数：`__clone()`、`__clone(__base*)`、`destroy()`、`destroy_deallocate()`、`operator()`、`target()`、`target_type()` |
| `__func<_Fp, Sig>` | `function.h:158` | 继承 `__base<Sig>`，持有 `_Fp __func_`；`__clone()` → `new __func(__func_)`（堆上克隆）；`__clone(__base* p)` → `::new (p) __func(__func_)`（就地克隆）；`destroy()` → `__func_.~_Fp()`；`destroy_deallocate()` → `delete this` |
| `__value_func<Sig>` | `function.h:192` | 默认路径：`__buf_`（24 字节 `aligned_storage<3*sizeof(void*)>`）+ `__f_`（`__base*`）；SBO 判断：`sizeof(_Fun) <= sizeof(__buf_) && is_nothrow_copy_constructible<_Fp>`；移动时堆路径偷指针，栈路径必须 `__clone` |
| `__policy_func<Sig>` | `function.h:424` | `_LIBCPP_ABI_OPTIMIZED_FUNCTION` 路径：`__policy_storage`（`char[16]` 联合 `void* __large`）+ `__policy*`（函数指针表：`__clone`、`__destroy`、`__is_null`、`__type_info`）；SBO 条件更严格：`sizeof(_Fun) <= 16 && alignof(_Fun) <= 8 && is_trivially_copyable && is_trivially_destructible` |
| `function<Sig>` | `function.h:607` | 薄壳：默认 `__value_func<Sig> __f_`（32 字节）或 `__policy_func<Sig> __f_`（24 字节）；所有操作委托 `__f_`；拷贝赋值采用 copy-and-swap 惯用法 |

### shared_ptr 侧

| 类型 / 函数 | 源码位置 | 说明 |
|-------------|----------|------|
| `__shared_count` | `shared_count.h:45` | `__shared_owners_`（初始 0）；`__add_shared()` → `__atomic_add_fetch(..., 1, __ATOMIC_RELAXED)`；`__release_shared()` → `__atomic_add_fetch(..., -1, __ATOMIC_ACQ_REL)`，返回 `true` 表示归零；`use_count()` → `__atomic_load_n(&__shared_owners_, __ATOMIC_RELAXED) + 1` |
| `__shared_weak_count` | `shared_count.h:81` | 继承 `__shared_count`（private）；新增 `__shared_weak_owners_`；`__release_shared()` → 归零时调 `__release_weak()`；`__release_weak()` → 弱计数归零时调 `__on_zero_shared_weak()`（释放控制块内存）；`lock()` → 原子检查 `use_count > 0` |
| `__shared_ptr_pointer<T,D,A>` | `shared_ptr.h:97` | `shared_ptr(new T(...))` 控制块：`_LIBCPP_COMPRESSED_TRIPLE(T, __ptr_, D, __deleter_, A, __alloc_)`；`__on_zero_shared()` → `__deleter_(__ptr_); __deleter_.~_Dp()`；`__on_zero_shared_weak()` → `__alloc_.deallocate(this)` |
| `__shared_ptr_emplace<T,A>` | `shared_ptr.h:133` | `make_shared` 控制块：内部 `_Storage` 包含 `_Data`（`_LIBCPP_COMPRESSED_PAIR(A, __alloc_, T, __elem_)`），布局为 `char[sizeof(_Data)]` 缓冲；`__on_zero_shared()` → `allocator_traits::destroy(__elem_)`；`__on_zero_shared_weak()` → `allocator_traits::deallocate(this)` |
| `shared_ptr<T>` | `shared_ptr.h:293` | 两个指针：`element_type* __ptr_` + `__shared_weak_count* __cntrl_`（16 字节）；可选 `_LIBCPP_SHARED_PTR_TRIVIAL_ABI` 属性优化寄存器传参 |

## 关键算法

### function：SBO 决策与克隆

| 触发条件 | 算发路径 |
|----------|----------|
| 构造 `function`（`__value_func` 路径） | 计算 `sizeof(__func<_Fp, Sig>)`（`__base` vtable ptr + `_Fp` 对象）；若 ≤ 24 且 `_Fp` 满足 `is_nothrow_copy_constructible` → `::new (&__buf_) _Fun(move(f))`（栈上就地构造）；否则 `new _Fun(move(f))`（堆分配） |
| 拷贝构造 | 比较 `src.__f_` 与 `src.__buf_` 地址：相等 → 栈路径，调 `__clone(__as_base(&__buf_))`（就地拷贝）；不等 → 堆路径，调 `__clone()` 返回新 `__base*` |
| 移动构造 | 栈路径（`(void*)src.__f_ == &src.__buf_`）→ 必须 `__clone`（不能偷指针，源对象需保持合法状态）；堆路径 → 直接 `this->__f_ = src.__f_; src.__f_ = nullptr`（指针窃取，O(1)） |
| `operator=` | 赋值采用 `function(move(f)).swap(*this)` 或 `*this = nullptr; ...` 模式：先销毁旧目标，再搬入新目标 |
| `swap` | 两个都在栈上 → 通过临时缓冲三次 `__clone` 交换；一栈一堆 → 栈到堆 `__clone` + 堆到栈指针窃取；两个都在堆 → 交换 `__f_` 指针 |

### shared_ptr：引用计数与两级析构

| 触发条件 | 算法路径 |
|----------|----------|
| `shared_ptr` 拷贝 | `__cntrl_->__add_shared()` → `__atomic_add_fetch(&__shared_owners_, 1, __ATOMIC_RELAXED)`；`use_count` 加一 |
| `shared_ptr` 析构（use_count > 1） | `__cntrl_->__release_shared()` → `__atomic_add_fetch(&__shared_owners_, -1, __ATOMIC_ACQ_REL)` → 未归零，返回 `false`，不触发析构 |
| `shared_ptr` 析构（use_count 归零） | `__release_shared()` 返回 `true` → 调 `__on_zero_shared()`（虚函数）：对 `__shared_ptr_emplace` → `allocator_traits::destroy(elem)`，对 `__shared_ptr_pointer` → `deleter(ptr)`；然后 `__release_weak()` |
| `weak_count` 归零（`__release_weak` 内） | `__shared_weak_owners_` 原子减到 -1 → 调 `__on_zero_shared_weak()`（虚函数）：对 `__shared_ptr_emplace` → `allocator_traits::deallocate(ctrl_blk)`，对 `__shared_ptr_pointer` → `alloc.deallocate(this)` |
| `weak_ptr::lock()` | 原子读 `use_count`：若 > 0，原子 `__add_shared()`（CAS loop 或 `__atomic_add_fetch`），返回非空 `shared_ptr`；若 == 0，返回空 `shared_ptr` |
| `make_shared` 构造 | 单次 `allocator_traits::allocate(alloc, 1)` 得到 `__shared_ptr_emplace<T, Alloc>` 的内存；构造时在 `_Storage` 的 `__buffer_` 中 placement-new alloc + `allocator_traits::construct(elem)`；析构时逆序 |

`__release_weak` 的非争用快速路径优化（`memory.cpp`）：

```cpp
// 非争用情况下用 acquire load 代替 atomicrmw
if (__atomic_load_n(&__shared_weak_owners_, __ATOMIC_ACQUIRE) == 0) {
  __on_zero_shared_weak();  // 无争用，直接释放
  return;
}
// 有争用：原子减，归零则释放
```

## ABI 约束

### function ABI

- **对象大小**：`sizeof(function<Sig>)` = `sizeof(__value_func<Sig>)` = 24 + 8 = 32 字节（`__buf_` 24 字节 + `__f_` 指针 8 字节，对齐后）；`__policy_func` 路径下为 24 字节（`__policy_storage` 16 + `__policy*` 8）
- **SBO 缓冲固定为 `3 × sizeof(void*)`**：改变此大小破坏所有已编译 `function` 对象的内存布局，属于 ABI break
- **`__base` 虚表**：每个被包装类型实例化一个新的 `__func` 子类，各自有独立虚表；同签名不同被包装类型的 `function` 共享 `__base` 接口但有独立 `__func` vtable
- **`_LIBCPP_ABI_OPTIMIZED_FUNCTION` 宏**：启用时切换到 `__policy_func` 路径，缩小对象大小但改变 SBO 条件（要求 trivially copyable + trivially destructible），与默认 ABI 不兼容
- **`bad_function_call`**：key function 的存在与否决定虚表是 exported 还是 weak；`_LIBCPP_AVAILABILITY_HAS_BAD_FUNCTION_CALL_KEY_FUNCTION` 宏控制此行为

### shared_ptr ABI

- **对象大小**：`sizeof(shared_ptr<T>)` = 16 字节（`__ptr_` + `__cntrl_` 各 8 字节）；`weak_ptr` 布局相同
- **`_LIBCPP_SHARED_PTR_TRIVIAL_ABI`**：可选的 `__attribute__((__trivial_abi__))`，允许 `shared_ptr` 通过寄存器传参（而非强制通过栈），显著减少调用开销；默认不启用，启用后与未编译此属性的代码 ABI 不兼容
- **控制块布局**：`__shared_count`（`long __shared_owners_`，8 字节）+ `__shared_weak_count`（新增 `long __shared_weak_owners_`，8 字节）+ 虚表指针（8 字节）= 控制块基础 24 字节；`__shared_ptr_emplace` 额外追加 `sizeof(CompressedPair<Alloc, T>)`
- **引用计数类型**：`long`（通常 8 字节于 64 位平台）；改变为其他类型属于 ABI break
- **`_LIBCPP_COMPRESSED_TRIPLE` / `_LIBCPP_COMPRESSED_PAIR`**：控制块内的压缩对布局由编译器宏 `_LIBCPP_ABI_NO_COMPRESSED_PAIR_PADDING` 控制；GCC 与 Clang 有不同的对齐策略以保持旧 ABI 兼容

## 异常安全

### function：基本保证

`__value_func` 的构造函数异常路径：

1. **SBO 路径**（`sizeof(_Fun) ≤ 24 && is_nothrow_copy_constructible`）：`_Fun` 的 move 构造由 `is_nothrow_copy_constructible` 担保；若 `_Fun` 自身的构造抛异常，`__f_` 保持 `nullptr`（初始状态），不会泄漏 → **基本保证**
2. **堆路径**（`new _Fun(move(f))`）：若 `operator new` 抛出 `bad_alloc`，`__f_` 保持 `nullptr`；若 `_Fun` 构造抛异常，`operator new` 自动回收内存 → **基本保证**
3. **`operator()` 调用**：用户可调用对象内部的异常直接传播，`__f_` 指向的内部状态不受影响；若 `__f_ == nullptr`，抛 `bad_function_call`

**注意**：SBO 要求 `is_nothrow_copy_constructible`（不仅是 move），原因是拷贝构造也需要安全——这保证了 `__clone(__base*)` 路径不会在栈上留下半构造状态。

### shared_ptr：强保证

1. **`make_shared<T>(args...)` 构造**：`allocator_traits::construct` 若抛异常，`__shared_ptr_emplace` 的 `_Storage` 通过 RAII 析构分配器，`allocator_traits::deallocate` 回收控制块内存 → **强保证**（单一分配路径，异常即全回滚）
2. **`shared_ptr(new T(...), deleter)` 构造**：元素 `new T(...)` 若抛异常，控制块已分配但会被自动释放（`__shared_ptr_pointer` 的构造器不分配控制块——控制块由 `shared_ptr` 构造函数分配后传入，构造函数用 `_Guard` RAII 对象保护）
3. **拷贝/赋值**：`shared_ptr` 的拷贝仅修改原子引用计数（`noexcept`），不会抛异常
4. **`shared_ptr` 的移动赋值**：先 `reset()` 旧控制块，再窃取新指针（`noexcept`）

**`shared_ptr` 是标准库中异常安全最强的组件之一**：除了初始构造时用户提供的构造函数/删除器可能抛异常外，所有操作均为 `noexcept`。

## 性能模型

### function 的性能特征

| 操作 | 有栈 SBO | 无栈（堆分配） |
|------|----------|---------------|
| 构造（无捕获 lambda / 函数指针） | 0 次 `malloc`，仅 placement new + 拷贝 | 1 次 `malloc`（`sizeof(__func)` + `__base` vtable ptr） |
| 构造（大 lambda / 捕获） | N/A | 1 次 `malloc` |
| 拷贝构造 | 1 次虚调用 `__clone(__base*)`（就地拷贝） | 1 次 `malloc` + 虚调用 `__clone()` |
| 移动构造 | 1 次虚调用 `__clone(__base*)`（必须拷贝，不可偷） | 0 次 `malloc`，指针窃取 |
| `operator()` | 1 次虚调用（`(*__f_)(args...)`） | 同左 |
| 析构（栈上） | 1 次虚调用 `destroy()`（就地析构，无 free） | N/A |
| 析构（堆上） | N/A | 1 次虚调用 `destroy_deallocate()`（`delete this`） |

**关键性能洞察**：

- `sizeof(function)` = 32 字节是所有主流实现中最大的（GCC 的 `function` = 32，MSVC 的 = 64），但这换来了 24 字节的 SBO 缓冲，使得 `int(*)(int)`、无捕获 lambda、小型函数对象（≤ 24 字节）全部免分配
- 移动栈上 callable 需要虚函数调用（与拷贝相同开销），这是 libc++ `function` 相比 `std::unique_ptr` 的核心代价
- `__policy_func`（`_LIBCPP_ABI_OPTIMIZED_FUNCTION`）的 SBO 条件更严（要求 trivially copyable + trivially destructible），但对象大小缩小到 24 字节，且避免虚函数开销（通过函数指针表）

### shared_ptr 的性能特征

| 操作 | 开销 |
|------|------|
| `make_shared<T>(args)` | 1 次 `malloc` + 1 次元素构造 |
| `shared_ptr(new T(args))` | 2 次 `malloc`（元素 + 控制块） |
| `shared_ptr` 拷贝 | 1 次 `atomic_add_fetch(RELAXED)` |
| `shared_ptr` 析构（use_count > 1） | 1 次 `atomic_add_fetch(ACQ_REL)` |
| `shared_ptr` 析构（use_count 归零） | 1 次 `atomic_add_fetch(ACQ_REL)` + 1 次虚调用 `__on_zero_shared()` + 1 次 `atomic_add_fetch(ACQ_REL)`（`__release_weak`） |
| `weak_ptr::lock()` | 1 次 acquire load + 1 次 `atomic_add_fetch(RELAXED)`（成功时） |

**缓存与内存考量**：

- `make_shared` 单次分配将控制块和元素放在同一缓存行，对弱引用场景有明显缓存友好优势
- 控制块虚表指针（8 字节）使得控制块访问至少触及一个缓存行（64 字节）；对高频 `shared_ptr` 拷贝/析构场景，控制块所在缓存行的争用是主要瓶颈
- `_LIBCPP_SHARED_PTR_TRIVIAL_ABI` 允许寄存器传参，避免 `shared_ptr` 按值传参时的栈溢出，对 IPC 密集场景有 ~10–15% 提升

## 编译 / benchmark 证据

### 编译时常量验证

```cpp
// function SBO 缓冲大小（64 位平台）
static_assert(sizeof(__function::__value_func<int(int)>) == 32, "sizeof(function) = 32");
static_assert(sizeof(std::function<int(int)>) == 32, "sizeof(function) = 32");

// shared_ptr / weak_ptr 大小
static_assert(sizeof(std::shared_ptr<int>) == 16, "shared_ptr = 2 pointers");
static_assert(sizeof(std::weak_ptr<int>) == 16, "weak_ptr = 2 pointers");

// SBO 阈值验证
// int(*)(int) = 8 字节 → SBO；大捕获 lambda → 堆
// __func<int(*)(int), int(int)> 包含 vtable(8) + ptr(8) = 16 ≤ 24 → SBO
```

### function SBO 覆盖率

| 类型 | sizeof(_Fun)（估算） | 进 SBO？ |
|------|---------------------|---------|
| `int(*)(int)`（函数指针） | 16（vtable + 指针） | ✅ 是 |
| `[=](int x){ return x + cap; }`（捕获 1 个 int） | 20（vtable + int + padding） | ✅ 是 |
| `[=](int x){ return x + a + b; }`（捕获 2 个 int） | 24（vtable + 2×int） | ✅ 是（边界） |
| `[=]() { return vec; }`（捕获 `std::vector`） | > 24 | ❌ 堆分配 |
| `std::bind(&obj, &Class::method, _1)` | > 24 | ❌ 堆分配 |

### shared_ptr 控制块开销

| 控制块类型 | 适用场景 | 元素分配 | 总 malloc 次数 |
|-----------|---------|---------|---------------|
| `__shared_ptr_emplace<T, Alloc>` | `make_shared<T>(args)` | 内联于控制块 | 1 |
| `__shared_ptr_pointer<T, D, A>` | `shared_ptr(new T, D)` | 独立 `new` | 2 |
| `__shared_ptr_pointer<T, D, A>` | `shared_ptr(new T)` | 独立 `new`，D = `default_delete` | 2 |

### 引用计数内存排序

| 操作 | 内存序 | 原因 |
|------|--------|------|
| `__add_shared()` | `__ATOMIC_RELAXED` | 仅递增，无后续依赖（`PR22803`） |
| `__release_shared()` | `__ATOMIC_ACQ_REL` | acquire 确保看到修改前的写入；release 确保自己的写入对下一个归零者可见 |
| `__add_weak()` | `__ATOMIC_RELAXED` | 同 `__add_shared` |
| `__release_weak()` | `__ATOMIC_ACQ_REL` | 归零时需要看到完整的对象析构结果 |