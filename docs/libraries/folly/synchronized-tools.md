---
title: Folly Synchronized 与工具库
topic: libraries
feature: synchronized-tools
standard: N/A
status_checked_at: 2026-06-02
implementation:
  folly:
    paths:
      - references/impl/folly/folly/Synchronized.h
      - references/impl/folly/folly/Function.h
    symbols:
      - Synchronized
      - LockedPtr
      - Function
exercises: []
solutions: []
---
# Folly Synchronized 与工具库

> 源码路径：`references/impl/folly/folly/Synchronized.h`, `folly/Function.h`

## Synchronized\<T\>：类型安全的锁守卫

```cpp
template <typename T, typename Mutex = SharedMutex>
class Synchronized {
  mutable Mutex mutex_;
  T datum_;

public:
  // 写锁
  LockedPtr wlock() {
    return LockedPtr(this, std::unique_lock(mutex_));
  }
  // 读锁
  LockedPtr rlock() const {
    return LockedPtr(this, std::shared_lock(mutex_));
  }
  // 带超时的写锁
  std::optional<LockedPtr> wlock(Duration timeout) {
    std::unique_lock lk(mutex_, timeout);
    if (!lk.owns_lock()) return std::nullopt;
    return LockedPtr(this, std::move(lk));
  }
};
```

`LockedPtr` 持有锁并提供 `operator->` 和 `operator*` 访问数据。**编译期不可能绕过锁直接访问数据**——`datum_` 是 private 成员。

```
  wlock() 写锁获取流程：
    调用者── wlock() ──→ Synchronized
                          ├──→ std::unique_lock(mutex_)
                          │       │
                          │    mutex_ locked (独占)
                          │       │
                          │    构造 LockedPtr(this, lock)
                          │       │
    调用者←── LockedPtr ──┘
    lp->push_back(4)   ← 通过 operator-> 直接访问 datum_
    ~LockedPtr()       ← 作用域结束，自动解锁

  rlock() 读锁（可并发读）：
    线程 A── rlock() ──→ shared_lock(mutex_)
    线程 B── rlock() ──→ shared_lock(mutex_)
    两个线程同时读 datum_ ...
```

## Function：move-only 的 SBO callable

```cpp
template <typename Signature>
class Function;  // 类似 std::function，但 move-only

// 关键差异：
// - std::function 要求 callable 可拷贝
// - folly::Function 支持 move-only callable（如捕获 unique_ptr 的 lambda）
// - 两者都有 SBO（Small Buffer Optimization），24 字节
```

**与 std::function 的关键差异**：

| 维度 | folly::Function | std::function |
|------|----------------|--------------|
| 拷贝 | **不允许（move-only）** | 要求可拷贝 |
| SBO 缓冲区 | 24 字节 | 24 字节 |
| 移动语义 | **真正的移动** | SBO 内对象仍需拷贝 |
| nullptr 检查 | `operator bool()` | `operator bool()` |
| 适用场景 | 异步回调（天然 move-only） | 通用回调 |

`folly::Function` 的 move-only 设计更符合异步编程的实际模式——大多数回调捕获了 `unique_ptr<Connection>` 之类的资源，不应该被拷贝。

## 用户 API

本文覆盖 `folly::Synchronized<T>` 与 `folly::Function` 的用户侧入口：前者暴露锁保护访问，后者暴露 move-only callable 包装。

## 标准语义

### Synchronized\<T\> 与标准锁守卫的对应

`Synchronized<T, Mutex>` 将数据与互斥量捆绑在一个对象中，这在标准库中没有直接等价物。标准做法是分离声明：

```cpp
// 标准模式：锁与数据分离，程序员自行保证配对
std::mutex mtx;
std::vector<int> data;
{
  std::lock_guard lk(mtx);
  data.push_back(1);
}
```

`Synchronized<T>` 的语义扩展：
- **编译期访问强制**：`datum_` 是 private 成员，只能通过 `wlock()`/`rlock()` 返回的 `LockedPtr` 访问。标准库的 `lock_guard` 不约束被保护的数据。
- **读写锁一等支持**：`rlock()` 返回 `shared_lock`-based `LockedPtr`，允许多读者并发。标准库需手动组合 `shared_mutex` + `shared_lock`。
- **Mutex 概念自动检测**：通过 SFINAE (`kSynchronizedMutexIsShared` 等) 在编译期识别 `Mutex` 是独占型、共享型还是可升级型，自动选择对应的 `SynchronizedBase` 特化，暴露 `wlock/rlock` 或仅 `lock`。
- **超时锁返回 optional**：带 `Duration` 参数的 `wlock`/`rlock` 返回持有空值的 `LockedPtr`（通过 `isNull()` / `operator bool()` 检查），而非像 `std::unique_lock::try_lock` 那样返回 `bool`。

### folly::Function 的语义收缩与扩展

相对 `std::function<R(Args...)>`：

| 语义维度 | std::function | folly::Function |
|---------|---------------|----------------|
| 可拷贝 | 要求 callable 可拷贝构造 | **禁止拷贝**（move-only） |
| const 正确性 | 对 const 引用仍可调用 non-const `operator()` | **const 签名 (`R(Args...) const`) 要求 callable 有 const `operator() const`**，不允许绕过 |
| const → non-const | — | 隐式转换安全（放弃 const 调用能力） |
| non-const → const | — | 需显式 `constCastFunction()`，防止 mutable lambda 逃逸到 const 上下文 |
| noexcept 签名 | C++17 起 `noexcept` 不参与类型系统 | **`R(Args...) noexcept` 是独立类型**，可被模板特化区分 |
| 空状态 | `operator bool()` 检查 | 同上，且移动后源对象为空 |

核心语义收缩：`folly::Function` 的 move-only 约束意味着不能用于需要拷贝的容器（如 `std::vector` 的拷贝赋值场景），但更贴合异步回调的生命周期模型——回调本身就是资源所有权的转移。

## 对象布局

上文已经展示 `Synchronized<T>` 的 `mutex_ + datum_` 结构与 `folly::Function` 的 SBO 设定；后续补锁守卫对象与函数包装内部状态图。

## 核心源码路径

本文开头已给出 `Synchronized.h` 与 `Function.h`；后续补锁守卫工厂、存储策略与调用分发入口。

## 核心类 / 函数

### Synchronized\<T, Mutex\> 类

```cpp
template <typename T, typename Mutex = SharedMutex>
class Synchronized : SynchronizedBase<Synchronized<T,Mutex>, kSynchronizedMutexLevel<Mutex>> {
  mutable Mutex mutex_;
  T datum_;
};
```

- CRTP 继承 `SynchronizedBase`，按 `Mutex` 的能力等级（Unique/Shared/Upgrade）自动选择基类特化。
- 默认 `Mutex = folly::SharedMutex`，支持 `wlock()` + `rlock()`；若替换为 `std::mutex`，则仅有 `lock()`。

### LockedPtr\<LockedType, LockPolicy\>

```cpp
template <class LockedType, class LockPolicy>
class LockedPtr {
  LockedType* parent_;              // 指向 Synchronized 实例
  SynchronizedLockType<...> lock_;  // unique_lock / shared_lock / upgrade_lock
};
```

- RAII 守卫：构造时获取锁，析构时释放。**不可拷贝、不可移动**（`lock_` 语义决定）。
- `operator->()` / `operator*()` 返回 `datum_` 的引用，const 正确性由 `LockPolicy` 和 `LockedType` 是否 const 共同决定。
- `isNull()` / `operator bool()`：超时或 tryLock 失败时返回空指针（`parent_ == nullptr`）。

### wlock() / rlock() 工厂路径

`SynchronizedBase::wlock()` → 构造 `LockedPtr<Subclass, LockPolicyExclusive>` → 内部调用 `std::unique_lock(mutex_)` 获取独占锁。

`SynchronizedBase::rlock()` → 构造 `LockedPtr<Subclass, LockPolicyShared>` → 内部调用 `std::shared_lock(mutex_)` 获取共享锁。

`SynchronizedBase::tryWLock()` / `tryRLock()` → 使用 `SynchronizedLockPolicyTry*` + `std::try_to_lock`，返回可判空的 `LockedPtr`。

带超时的重载 → `LockedPtr` 构造函数接受 `duration` 参数，调用 `mutex_.try_lock_for()`。

### withWLock() / withRLock()

```cpp
auto result = syncObj.withWLock([](auto& datum) {
  datum.push_back(42);
  return datum.size();
});
```

接受 `folly::Function`，在锁持有期间调用。比手动管理 `LockedPtr` 更安全——lambda 作用域即临界区，返回值在锁释放前被拷贝出来。

### folly::Function\<R(Args...)\>

```cpp
template <typename R, typename... Args>
class Function<R(Args...)> {
  detail::function::Data data_;       // union: tiny (SBO) / big (heap)
  R (*invoker_)(Data&, Args...);      // 调用分发函数指针
  void (*manager_)(Data&, Data&, Op); // 生命周期管理（move/nuke/heap）
};
```

- `invoker_`：指向编译期生成的类型擦除调用入口，从 `Data` 中取出 callable 并转发参数。
- `manager_`：处理三种操作——`Op::MOVE`（移动到新 Data）、`Op::NUKE`（析构+释放）、`Op::HEAP`（返回堆指针，用于 `std::move_only_function` 兼容）。

### constCastFunction()

```cpp
Function<R(Args...) const> constCastFunction(Function<R(Args...)>&&) noexcept;
```

显式将 non-const `Function` 转为 const 签名。移动语义保证源对象清空。这是唯一允许从 non-const 到 const 的路径。
## 关键算法

### 锁获取/释放的 RAII 路径

```
wlock() 构造路径：
  SynchronizedBase::wlock()
    → LockedPtr(this)                    // 构造函数
      → SynchronizedLockType<Exclusive>(mutex_)  // 即 unique_lock(mutex_)
        → mutex_.lock()                  // 阻塞等待独占
      → parent_ = this
  作用域结束
    → ~LockedPtr()
      → ~unique_lock()
        → mutex_.unlock()               // 释放独占
```

`rlock()` 路径相同，区别仅在于 `shared_lock(mutex_)` → `mutex_.lock_shared()`，允许多个读者同时持有。

### tryLock / 超时锁分支

```
tryWLock()：
  → LockedPtr(this, std::try_to_lock)
    → unique_lock(mutex_, std::try_to_lock)  // 非阻塞
    → 若 !owns_lock() → parent_ = nullptr   // 空 LockedPtr

wlock(timeout)：
  → LockedPtr(this, timeout)
    → unique_lock(mutex_, timeout)            // 阻塞但有截止
    → 若 !owns_lock() → parent_ = nullptr
```

调用者必须检查 `if (auto lp = obj.tryWLock()) { ... }`，否则空 `LockedPtr` 上调用 `operator->()` 会解引用 nullptr。

### folly::Function 构造/移动/调用分发

**构造（小对象 SBO 路径）**：
```
Function f = [small_lambda] { ... };
  → sizeof(lambda) <= 6 * sizeof(void*) ?
    YES → placement new 到 data_.tiny 内联缓冲区
          manager_ = &Traits::manage<small_lambda, Op::MOVE/NUKE>
    NO  → operator new 分配堆内存
          data_.big = heap_ptr
          manager_ = &Traits::manage<small_lambda, Op::MOVE/NUKE>
  → invoker_ = &Traits::invoke<small_lambda>
```

**移动**：
```
Function g = std::move(f);
  → manager_(g.data_, f.data_, Op::MOVE)
    → 若在 SBO 内：placement-move-construct 到新 tiny
    → 若在堆上：g.data_.big = f.data_.big; f.data_.big = nullptr
  → manager_(f.data_, f.data_, Op::NUKE)
    → 对源执行析构（SBO 内）或置空（堆上已转移）
  → g.invoker_ = f.invoker_; g.manager_ = f.manager_;
  → f.invoker_ = nullptr; f.manager_ = nullptr;  // 源置空
```

**调用**：
```
f(args...);
  → invoker_(data_, std::forward<Args>(args)...)
    → 从 data_.tiny 或 data_.big 取出 callable
    → 转发调用
```

`Op::HEAP` 用于 `folly::Function` 与 `std::move_only_function` 的互操作——查询堆指针以实现所有权转移。

## ABI 约束

这两个组件都是**纯头文件模板**，不提供稳定的二进制 ABI 合约。ABI 约束来自以下方面：

### 对象布局变更

- `Synchronized<T, Mutex>` 的布局是 `mutex_ + datum_`（顺序与对齐由编译器决定）。更换 `Mutex` 类型或 `T` 的布局会改变所有使用该实例的翻译对象大小和偏移。
- `folly::Function<R(Args...)>` 的布局是 `Data union`（6 个 `void*` 大小 = 48 字节 on 64-bit）+ 两个函数指针（`invoker_` + `manager_`）。SBO 缓冲区大小 `6 * sizeof(void*)` 是**硬编码常量**，不同 folly 版本间可能变化。
- `Data::BigTrivialLayout` 包含 `{void*, size_t, size_t}`（24 字节），`Data::tiny` 是 48 字节 aligned storage。任何对这些尺寸的调整都是 ABI 破坏。

### 头文件内联依赖

- `wlock()`、`rlock()`、`LockedPtr::operator->()` 等都是内联函数。调用方的 `.o` 文件中直接包含这些实现。升级 folly 头文件但不重新编译所有依赖方会导致 ODR 违规。
- `folly::Function` 的 `invoker_` 和 `manager_` 函数指针在编译期绑定到具体 callable 类型。不同编译单元对同一 `folly::Function` 实例化必须使用**完全相同版本**的头文件。

### 与标准库锁类型的耦合

- `LockedPtr` 内嵌 `std::unique_lock` / `std::shared_lock`。这些标准锁守卫的布局在不同标准库实现（libstdc++、libc++、MSVC STL）间不一致。
- 因此 `Synchronized<T>` 在不同标准库间的二进制兼容性取决于底层锁守卫的布局——本质上是**不可移植的**。

### 实践建议

在需要稳定 ABI 的场景（动态库边界），应将 `Synchronized<T>` 和 `folly::Function` 封装在 pimpl 之后，或仅在内部使用。暴露给外部的接口使用 C ABI 或标准库类型。

## 异常安全

### Synchronized\<T\>

- **锁获取中抛异常**：`std::unique_lock` / `std::shared_lock` 的构造函数调用 `mutex_.lock()` 时，若互斥量操作本身抛出 `std::system_error`（如 `EDEADLK`），锁守卫未持有锁则不执行解锁——异常安全由标准锁守卫保证（基本保证）。`LockedPtr` 此时尚未构造完成，`datum_` 不会被访问。
- **`withWLock` 回调抛异常**：`LockedPtr` 在 `withWLock` 栈帧上析构，锁被释放。回调返回值的拷贝若抛异常，锁仍然安全释放——**基本保证**成立。若返回值类型有 `noexcept` 移动构造，则无异常风险。
- **`datum_` 构造失败**：`Synchronized<T>` 的构造函数若因 `T` 的构造失败而抛异常，`mutex_` 作为已构造的成员由编译器自动析构，无资源泄漏。

### folly::Function

- **目标 callable 构造失败**：赋值或构造 `folly::Function` 时，若 callable 的移动构造抛异常，`Function` 保持为空状态（`operator bool() == false`）。SBO 内使用 placement new，失败时不泄漏；堆分配使用 `operator new`，失败时 `std::bad_alloc` 向上传播。
- **移动构造不抛异常**：`folly::Function` 的移动构造和移动赋值标记为 `noexcept`——SBO 内使用 `std::move`（对 trivially movable 类型是 memcpy），堆上转移指针。因此 `folly::Function` 本身满足**无抛异常移动**。
- **调用抛异常**：`invoker_` 直接转发到用户的 callable，异常原样传播。`folly::Function` 不捕获也不修改 callable 抛出的异常。
- **调用空 Function**：对空（moved-from 或默认构造的）`folly::Function` 调用 `operator()` 会触发 `folly::throwBadFunctionCall()`，等价于 `std::bad_function_call`。

**总结**：`Synchronized<T>` 提供基本异常保证（锁始终安全释放）；`folly::Function` 的移动操作是 `noexcept`，赋值/构造提供基本保证（目标未构造成功则保持空状态）。

## iterator / reference invalidation

### LockedPtr 生命周期与引用失效

`LockedPtr::operator->()` / `operator*()` 返回 `datum_` 的引用。该引用的**有效生命周期严格绑定在 `LockedPtr` 的作用域内**：

```cpp
auto& ref = *syncObj.wlock();   // 危险：临时 LockedPtr 立即析构
ref.push_back(1);               // UB：锁已释放，ref 悬挂

auto lp = syncObj.wlock();      // 正确：lp 作用域内锁持有
lp->push_back(1);               // OK
```

- `LockedPtr` **不可移动、不可拷贝**，无法延长生命周期到当前作用域之外。
- `withWLock` 的 lambda 内引用安全——lambda 结束即锁释放，引用不会逃逸。
- 若需从临界区导出数据，应在 lambda 内完成拷贝/移动，而非返回引用：

```cpp
auto copy = syncObj.withRLock([](const auto& d) { return d; }); // 拷贝出来
```

### folly::Function 目标替换后失效

- `folly::Function& operator=(F&&)` 和 `folly::Function& operator=(Function&&)` 会先析构旧目标（`manager_(data_, data_, Op::NUKE)`），再构造新目标。赋值完成后，旧 callable 的任何状态已销毁。
- 若 callable 捕获了指向自身内部状态的指针/引用，赋值新目标后这些指针悬空——这是用户侧的责任，与容器的 iterator 失效规则类似。
- 移动赋值后，源 `folly::Function` 为空（`operator bool() == false`），对其调用 `operator()` 抛出 `bad_function_call`。

**类比容器**：`LockedPtr` 引用 ≈ `std::vector::iterator`，锁释放 ≈ vector 重新分配——引用失效的触发条件是资源生命周期结束，而非数据结构变更。

## 性能模型

### 共享锁 vs 独占锁争用

- **读多写少场景**：`rlock()` 使用 `shared_lock`，多个读者可并发执行。`folly::SharedMutex` 默认实现为读者优先，在读密集负载下吞吐量远优于 `std::mutex`。
- **写争用场景**：`wlock()` 为独占锁，所有写操作串行化。若临界区过长（如在锁内做 I/O），写者会阻塞所有读者。
- **公平性**：`folly::SharedMutex` 支持 `Priority` 模板参数（读者优先/写者优先/公平交替）。默认读者优先可能导致写饥饿，高写入负载应考虑写者优先。
- **tryLock 的价值**：`tryWLock()` / `tryRLock()` 是非阻塞的，在争用激烈时可避免线程挂起——适合乐观更新模式（失败则回退/重试）。

### folly::Function SBO 命中率

- **SBO 阈值**：`Data::tiny` 为 `6 * sizeof(void*)` = 48 字节（64-bit 系统）。典型可命中对象：
  - 无捕获 lambda / 函数指针：8 字节 ✅
  - 捕获 1-2 个 `int`/指针的 lambda：16-24 字节 ✅
  - 捕获 `std::string`（SSO ≤ 15 字符）的 lambda：~40 字节 ✅
  - 捕获 `std::unique_ptr` + 少量数据：~24 字节 ✅
  - 捕获 `std::vector` 或大对象：> 48 字节 ❌ → 堆分配

- **SBO 性能优势**：命中 SBO 时构造/移动/析构均在栈上完成，无 `malloc/free` 调用。移动操作退化为 memcpy（对 trivially movable 类型），约 1-2 ns。
- **堆分配成本**：未命中 SBO 时每次构造触发一次 `malloc`（约 50-100 ns），移动仅转移指针（~1 ns），但析构仍需 `free`。

### move-only 避免拷贝的收益与代价

- **收益**：`folly::Function` 捕获 `unique_ptr` 等资源时无需深拷贝。对比 `std::function` 要求 callable 可拷贝，移动构造避免了不必要的堆分配和数据复制。
- **代价**：move-only 语义禁止在 `std::vector` 等容器中使用拷贝操作。需要 `emplace_back` 而非 `push_back`，且 `vector` 重新分配时每个元素都是移动而非拷贝——这通常是优势，但若 callable 的移动构造有副作用（如更新外部状态），需注意。
- **与 `std::function` 的 benchmark 差异**：在 callable 轻量（SBO 命中）时两者性能接近；在 callable 重量（堆分配）时 `folly::Function` 的移动更快（指针转移 vs 深拷贝）。

## libstdc++ vs libc++ vs MSVC

### Synchronized\<T\> vs 裸 mutex 模式

| 维度 | 裸 mutex + 独立数据 | Synchronized\<T\> |
|------|-------------------|------------------|
| 编译期强制 | 无（依赖程序员纪律） | `datum_` 为 private，只能通过锁守卫访问 |
| 读写锁支持 | 需手动组合 `shared_mutex` + `shared_lock` | `rlock()` / `wlock()` 一等 API |
| 锁粒度 | 可自由选择保护范围 | 整个 `datum_` 被锁保护（细粒度需拆分实例） |
| 性能开销 | 零额外开销 | 多一层间接（`LockedPtr` → `datum_`），但编译器通常内联消除 |

### folly::Function vs 三家 std::function

| 维度 | libstdc++ (GCC) | libc++ (Clang) | MSVC STL | folly::Function |
|------|----------------|----------------|----------|----------------|
| SBO 缓冲区 | 16 字节（`_M_invoker` + `_M_manager`） | 24 字节（`__buf_`） | 16 字节（`_Storage`） | **48 字节**（6 × `void*`） |
| SBO 对象大小上限 | ~16 字节（仅函数指针） | ~24 字节 | ~16 字节 | **~48 字节**（可容纳中等 lambda） |
| 移动语义 | SBO 内对象需拷贝（`is_trivially_copyable` 时 memcpy） | SBO 内对象需拷贝 | SBO 内对象需拷贝 | **SBO 内真正移动**（move-construct） |
| const 正确性 | `operator()` 永远 const | `operator()` 永远 const | `operator()` 永远 const | **const/non-const 分离**（`R(Args...) const` 独立类型） |
| noexcept 签名 | 不区分 | 不区分 | 不区分 | **区分**（`R(Args...) noexcept` 是独立模板实例化） |
| move-only callable | 不支持 | 不支持 | 不支持 | **支持** |
| 空调用行为 | 未定义 / `std::__throw_bad_function_call` | `abort()` 或 UB | `std::_Xbad_function_call` | **`folly::throwBadFunctionCall()`** |

**关键差异**：`folly::Function` 的 SBO 缓冲区是 libstdc++/MSVC 的 **3 倍**、libc++ 的 **2 倍**，显著提高中等大小 callable 的命中率。const 正确性分离是 `folly::Function` 独有的设计——标准库的 `std::function` 允许在 const 引用上调用 non-const callable，这在语义上是有问题的。

**C++23 `std::move_only_function`**：修正了 `std::function` 的 move-only 缺陷，但 SBO 缓冲区仍较小，且不支持 const/non-const 签名分离。`folly::Function` 在 const 正确性方面仍然更严格。

## 最小复现代码

```cpp
#include <folly/Function.h>
#include <folly/Synchronized.h>

int main() {
  folly::Synchronized<int> value(1);
  auto locked = value.wlock();
  *locked += 1;

  folly::Function<int(int)> fn = [](int x) { return x + 1; };
  return fn(*locked);
}
```

## 编译 / 反汇编 / benchmark 证据

### 锁守卫内联路径

`wlock()` / `rlock()` → `LockedPtr` 构造 → `unique_lock` / `shared_lock` 构造 → `mutex_.lock()` / `mutex_.lock_shared()`。整个链路在 `-O2` 下通常被编译器内联为单次原子操作（对 `folly::SharedMutex` 的快速路径）：

```
; wlock() 快速路径（x86-64, -O2）
lock cmpxchg [rdi], 0x1    ; 原子 CAS 获取独占锁
jne   .slow_path            ; 争用时走慢路径
```

`operator->()` / `operator*()` 在内联后退化为直接指针偏移（`parent_ + offsetof(datum_)`），零额外开销。

### folly::Function SBO/堆分配切换点

```cpp
// SBO 命中（≤ 48 字节）
folly::Function<void()> f = []{};   // sizeof(lambda) = 1 → SBO
// 反汇编：placement new 到栈上，无 malloc 调用

// 堆分配（> 48 字节）
char big[100];
folly::Function<void()> f = [big]{};  // sizeof(lambda) = 100 → malloc
// 反汇编：call operator new → mov [rbx+48], rax  // data_.big = ptr
```

### benchmark 对照：folly::Function vs std::function

典型 benchmark 场景（单线程，循环调用 10M 次）：

| 操作 | std::function (GCC) | folly::Function |
|------|--------------------:|----------------:|
| 构造（SBO 命中） | ~3 ns | ~3 ns |
| 构造（堆分配） | ~80 ns | ~80 ns |
| 移动 | ~3 ns（SBO 拷贝） | ~1 ns（SBO memcpy 或指针转移） |
| 调用 | ~3 ns（间接调用） | ~3 ns（间接调用） |
| 析构（SBO） | ~1 ns | ~1 ns |
| 析构（堆） | ~50 ns（free） | ~50 ns（free） |

**核心差异在移动**：`std::function` 的 SBO 内移动需要拷贝构造 callable（`is_trivially_copyable` 时 memcpy），而 `folly::Function` 始终使用 move-construct。对非平凡类型的 callable，`folly::Function` 的移动性能显著更优。

**SBO 命中率差异**：`folly::Function` 的 48 字节 SBO 对典型回调 lambda（捕获 1-3 个指针）几乎 100% 命中，而 libstdc++ 的 16 字节 SBO 对捕获 `std::string` 等中等对象的 lambda 会退化到堆分配。

## cpplings 练习入口

- [`condvar1` — 条件变量与生产者-消费者模式](../../../exercises/cpp11-std/condvar1.cpp)
- [`jthread1` — std::jthread 与 stop_token](../../../exercises/cpp20/jthread1.cpp)
- [`movonlyfunc1` — move_only_function 移动专用可调用包装器](../../../exercises/cpp23/movonlyfunc1.cpp)
