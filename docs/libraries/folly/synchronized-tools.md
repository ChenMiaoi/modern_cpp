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
