# C++20 协程 (Coroutines)

## 概述

C++20 引入语言级别无栈协程（stackless coroutine），通过 `co_await`、`co_yield`、`co_return` 三个关键字控制协程的**挂起**与**恢复**。编译器将协程执行状态保存到堆上的协程帧（coroutine frame）中，挂起后可随时恢复继续执行。标准库直到 C++23 才提供 `std::generator`；C++20 的协程框架是可定制的底层原语——生产中需自定义返回类型或借助第三方库（如 `cppcoro`、Boost.Asio）。核心价值：以同步风格写异步逻辑、惰性生成器（按需计算、O(1) 内存）、高并发任务调度（无线程切换开销）。

## 协程关键字与挂起点

三个关键字定义协程行为：`co_await expr` 挂起等待 Awaiter 就绪；`co_yield expr` 等价于 `co_await promise.yield_value(expr)` 产出值并挂起；`co_return expr` 调用 `return_value(expr)`（无参时调用 `return_void()`）并结束协程体。

`co_await expr` 的求值构成一个**挂起点**（suspension point）。编译器按 Awaiter 协议调用三个方法：

```cpp
struct Awaiter {
    bool await_ready() const noexcept;     // true → 跳过挂起，立即继续
    // await_suspend 三种返回类型：
    //   void              → 无条件挂起
    //   bool              → false 时立即恢复（条件挂起）
    //   coroutine_handle  → 对称转移（见下文）
    auto await_suspend(std::coroutine_handle<>) const noexcept;
    auto await_resume() const noexcept;    // 恢复后返回值
};
```

标准库提供 `std::suspend_always`（永远挂起）和 `std::suspend_never`（永不挂起）。

```cpp
Generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) { co_yield a; auto tmp = a; a = b; b = tmp + b; }
}
Task<int> compute() {
    int x = co_await async_read();  // 挂起等待异步 I/O
    co_return x * 2;
}
```

## promise_type

每个协程返回类型必须内嵌或通过 trait 关联一个 `promise_type`，定义协程完整生命周期协议：

```cpp
struct Task {
    struct promise_type {
        int result{};
        Task get_return_object() {                    // 构造返回对象
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }  // 惰性启动
        std::suspend_always final_suspend() noexcept { return {}; }    // 最终挂起
        void return_value(int v) { result = v; }      // co_return expr
        // void return_void() noexcept {}              // co_return; 或无 co_return 时
        std::suspend_always yield_value(int v) {       // co_yield expr
            result = v; return {};
        }
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
};
```

关键约束：`return_value` 与 `return_void` **不能共存**；`co_yield` 存在时隐式允许 `return_void`。`final_suspend()` 返回的 Awaiter 必须可挂起（`await_ready()` 为 `false`），否则协程帧自动销毁后句柄无效（UB）。

## coroutine_handle

`std::coroutine_handle<Promise>` 是非拥有型句柄，提供对协程帧的操作：

```cpp
auto h = task.handle;
h.resume();       // 从挂起点恢复执行
h.done();         // 查询是否在最终挂起点
h.destroy();      // 销毁协程帧（必须在挂起状态）
h.promise();      // 访问 promise 对象（需具体 Promise 类型）
// 类型擦除版本：可 resume，无法访问 promise
std::coroutine_handle<void> erased = h;
// 空操作句柄，用于终止对称转移链
std::coroutine_handle<> noop = std::noop_coroutine();
```

`coroutine_handle` 不管理生命周期。典型做法是 RAII 包装器在析构函数中调用 `destroy()`。

## Generator 模式

C++20 需手写 Generator，C++23 提供 `std::generator<T>`：

```cpp
template <typename T>
class Generator {
public:
    struct promise_type {
        T current{};
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
        std::suspend_always yield_value(T value) {
            current = std::move(value); return {};
        }
    };
    struct iterator {
        std::coroutine_handle<promise_type> h;
        iterator& operator++() { h.resume(); return *this; }
        const T& operator*() const { return h.promise().current; }
        bool operator==(std::default_sentinel_t) const { return !h || h.done(); }
    };
    iterator begin() { if (handle) handle.resume(); return {handle}; }
    std::default_sentinel_t end() { return {}; }
    ~Generator() { if (handle) handle.destroy(); }
    Generator(Generator&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
    Generator(const Generator&) = delete;
private:
    explicit Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
    std::coroutine_handle<promise_type> handle;
};
Generator<int> range(int lo, int hi) {
    for (int i = lo; i < hi; ++i) co_yield i;
}
// for (int v : range(0, 10)) { /* 0, 1, ..., 9 */ }
```

`begin()` 首次 `resume()` 推进到第一个 `co_yield`；每次 `++` 推进到下一个挂起点。C++23 可直接使用 `std::generator<T>` 替代手写版本。

## Async 模式

将底层异步操作包装为 Awaiter，以同步风格编写异步代码：

```cpp
struct AsyncReadAwaiter {
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        register_io_callback(fd, [h]() { h.resume(); });
    }
    ssize_t await_resume() { return get_read_result(fd); }
};
Task<ssize_t> handle_connection(int fd) {
    ssize_t n = co_await AsyncReadAwaiter{fd};  // 挂起等待 I/O 就绪
    co_return n;
}
```

## 对称转移 (Symmetric Transfer)

嵌套 `resume()` 导致调用栈线性增长。对称转移让 `await_suspend` 返回 `coroutine_handle`，编译器优化为尾调用，栈深度恒定：

```cpp
struct Task {
    struct promise_type {
        std::coroutine_handle<> continuation{};
        // ... get_return_object, initial_suspend, return_void, unhandled_exception ...
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                    return h.promise().continuation
                        ? h.promise().continuation : std::noop_coroutine();
                }
            };
            return FinalAwaiter{};
        }
    };
    std::coroutine_handle<promise_type> handle;
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<> caller) noexcept {
        handle.promise().continuation = caller;
        return handle;  // 对称转移：直接跳转，不增长调用栈
    }
};
```

`final_suspend` 返回 `continuation` 将控制权交还调用者，避免 `A.resume() → B.resume() → C.resume()` 的栈溢出。

## 最佳实践

1. **惰性启动**：`initial_suspend()` 返回 `suspend_always`，避免构造后立即执行到挂起点导致的竞态。
2. **最终挂起**：`final_suspend()` 的 `await_ready()` 必须返回 `false`；否则句柄自动销毁后再访问是 UB。
3. **异常传播**：在 `unhandled_exception()` 中用 `std::current_exception()` 捕获并存储，在 `await_resume()` 中重新抛出。
4. **RAII 管理句柄**：返回对象的析构函数中调用 `destroy()`；禁止裸句柄泄漏。
5. **对称转移**：长协程链必须使用对称转移避免栈溢出，以 `noop_coroutine()` 作为终止哨兵。
6. **HALO 验证**：编译器可将协程帧分配到调用者栈上（HALO 优化），但这是优化而非保证，性能敏感路径需 benchmark。

## 常见陷阱

1. **悬空引用**：`co_await` 是潜在挂起点，挂起后所有引用捕获的外部对象必须仍然存活：
   ```cpp
   Task dangerous(const std::string& input) {
       co_await some_async_op();  // input 可能在外部已析构
       process(input);            // UB
   }
   // 修复：参数按值传递，或在挂起前拷贝到局部变量
   ```
2. **堆分配与 HALO**：协程帧通常由编译器通过 `operator new` 堆分配。HALO 可在生命周期可静态分析时消除堆分配，但惰性协程比立即执行的协程更难被优化。
3. **`initial_suspend` 与竞态**：使用 `suspend_never` 立即执行时调用者可能尚未就绪。异步任务**强烈建议惰性启动**。
4. **`promise_type` 成员遗漏**：缺少任一必须成员（`get_return_object`、`initial_suspend`、`final_suspend`、`return_value` 或 `return_void`、`unhandled_exception`）将导致晦涩的模板错误。建议从最小可编译模板逐步扩展。
5. **`done()` 误判**：协程在 `initial_suspend` 挂起时 `done()` 为 `false`，但尚未产出任何值；在 `final_suspend` 挂起时 `done()` 为 `true`。迭代器必须同时检查 `h.done()` 和句柄有效性。
