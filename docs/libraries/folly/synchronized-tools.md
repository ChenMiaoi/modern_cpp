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

待补：补上 `Synchronized<T>` 与标准锁守卫语义的对应关系，以及 `folly::Function` 相对 `std::function` 的语义收缩与扩展。

## 对象布局

上文已经展示 `Synchronized<T>` 的 `mutex_ + datum_` 结构与 `folly::Function` 的 SBO 设定；后续补锁守卫对象与函数包装内部状态图。

## 核心源码路径

本文开头已给出 `Synchronized.h` 与 `Function.h`；后续补锁守卫工厂、存储策略与调用分发入口。

## 核心类 / 函数

待补：统一整理 `Synchronized<T>`、`LockedPtr`、`wlock/rlock`、`folly::Function` 与其内部 manager/invoker 路径。

## 关键算法

待补：补上锁获取/释放的 RAII 路径、超时锁分支，以及 move-only callable 的构造/移动/调用分发流程。

## ABI 约束

待补：说明这两类模板类型更受头文件内联与对象布局变更约束，而不是稳定二进制 ABI 合约。

## 异常安全

待补：补充锁构造失败、超时返回、`folly::Function` 目标构造失败和移动后空状态的保证等级。

## iterator / reference invalidation

待补：本文主题不是容器 iterator；后续这里补 `LockedPtr` 生命周期结束后外部引用不可继续使用，以及 `folly::Function` 目标替换后的句柄失效边界。

## 性能模型

待补：补上共享锁/独占锁争用、SBO 命中率、move-only callable 避免拷贝的收益与代价。

## libstdc++ vs libc++ vs MSVC

待补：这里主要与标准库做语义对照——`Synchronized<T>` 对比裸 mutex 模式，`folly::Function` 对比三家 `std::function` 的 SBO / 调度实现。

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

待补：补上锁守卫内联路径、`folly::Function` SBO/堆分配切换点与 `std::function` 的 benchmark 对照。

## cpplings 练习入口

- [`condvar1` — 条件变量与生产者-消费者模式](../../../exercises/cpp11-std/condvar1.cpp)
- [`jthread1` — std::jthread 与 stop_token](../../../exercises/cpp20/jthread1.cpp)
- [`movonlyfunc1` — move_only_function 移动专用可调用包装器](../../../exercises/cpp23/movonlyfunc1.cpp)
