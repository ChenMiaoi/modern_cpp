# 并发与内存模型术语

## Data Race vs Race Condition

**Data Race**：两个线程同时访问同一内存位置，至少一个是写入，且没有同步。这是 **UB**。

**Race Condition**：程序结果依赖于线程执行的相对顺序。不是 UB，但是 bug。

```cpp
// Data Race（UB！）
int counter = 0;
std::thread t1([&]{ counter++; });
std::thread t2([&]{ counter++; });

// Race Condition（逻辑 bug，不是 UB）
bool ready = false;
std::thread producer([&]{ data = 42; ready = true; });  // 可能乱序
std::thread consumer([&]{ while(!ready); use(data); }); // 可能看到 data=0
```

## Happens-Before

C++ 内存模型的核心关系——如果 A happens-before B，则 A 的效果对 B 可见：

```
- 同一线程内：A 在 B 之前 → A sequenced-before B → A happens-before B
- 跨线程：A release → B acquire（同一个 atomic 变量）→ A happens-before B
- 传递性：A happens-before B 且 B happens-before C → A happens-before C
```

## Memory Order

### relaxed

只保证原子性，不保证顺序：

```cpp
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);
// 保证原子递增，但不保证其他线程立即看到结果
```

### acquire / release

配对使用，建立 happens-before 关系：

```cpp
// 线程 1：写数据 + release
data = 42;
flag.store(true, std::memory_order_release);

// 线程 2：acquire + 读数据
if (flag.load(std::memory_order_acquire)) {
  use(data);  // 保证看到 data = 42
}
```

### seq_cst（默认）

最强的顺序——所有线程看到相同的操作顺序。性能最差但最安全。

## Lock-Free（无锁）

一个数据结构是 lock-free 的，如果至少有一个线程能在有限步内完成操作（即使其他线程被挂起）：

```cpp
std::atomic<int> counter;
// lock-free：fetch_add 总是能在有限步完成
```

## ABA Problem

无锁算法中的经典陷阱：

```
线程 1: 读取 A → 计算新值 → CAS(A, B)
线程 2: 在线程 1 的 CAS 之前，将 A 改为 B 又改回 A
线程 1: CAS 成功——但数据已经被修改过了！
```

解决方案：使用带版本号的指针、`std::shared_ptr` 的 atomic 操作。

## False Sharing（伪共享）

两个线程访问不同的变量，但它们恰好在同一 cache line 中：

```cpp
struct Bad {
  int thread1_data;  // 假设在 cache line X
  int thread2_data;  // 也在 cache line X！
  // 两个线程修改不同变量，但 cache line 失效导致性能下降
};

struct Good {
  alignas(64) int thread1_data;  // 独占一个 cache line
  alignas(64) int thread2_data;  // 独占另一个 cache line
};
```

Cache line 通常是 64 字节。`alignas(64)` 确保变量对齐到 cache line 边界。

## Memory Barrier（内存屏障）

告诉 CPU 不要重排特定的内存操作。`std::atomic` 的 memory order 在底层编译为内存屏障指令。

```
x86/x64: 天然强内存模型，大部分重排被硬件禁止
ARM/POWER: 弱内存模型，需要显式屏障
```

这就是为什么在 x86 上 `memory_order_relaxed` 看起来"正常工作"，但在 ARM 上会暴露问题。
