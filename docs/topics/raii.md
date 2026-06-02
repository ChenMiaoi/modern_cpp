---
title: RAII 与资源管理
topic: topics
feature: raii
status_checked_at: 2026-06-01
standard: N/A
---

# RAII 与资源管理

## RAII 原则

RAII（Resource Acquisition Is Initialization）是 C++ 最重要的惯用法：**资源的生命周期绑定到对象的生命周期**——构造函数获取资源，析构函数释放资源。C++ 保证局部对象在离开作用域时析构（包括栈展开期间），因此 RAII 在资源由对象所有且发生正常作用域退出或栈展开时，系统性地避免了手写释放路径导致的泄漏。

RAII 覆盖的"资源"远不止内存：文件描述符、互斥锁、网络连接、图形上下文、数据库连接。

```cpp
// 没有 RAII — 每个退出点都要手动释放
void bad() {
    FILE* f = fopen("data.txt", "r");
    if (!f) return;
    char buf[256];
    if (!fgets(buf, 256, f)) { fclose(f); return; }
    fclose(f);
}

// RAII — 不可能泄漏
void good() {
    auto f = std::unique_ptr<FILE, decltype(&fclose)>(
        fopen("data.txt", "r"), &fclose);
    if (!f) return;
    char buf[256];
    fgets(buf, 256, f.get());
} // 析构自动关闭
```

## 构造函数与析构函数

```cpp
class FileHandle {
    int fd_ = -1;
public:
    explicit FileHandle(const char* path, int flags)
        : fd_(open(path, flags)) {
        if (fd_ < 0)
            throw std::system_error(errno, std::system_category(), "open failed");
    }
    ~FileHandle() { if (fd_ >= 0) ::close(fd_); }

    // 移动语义：转移资源所有权
    FileHandle(FileHandle&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }
    FileHandle& operator=(FileHandle&& o) noexcept {
        if (this != &o) { if (fd_ >= 0) ::close(fd_); fd_ = o.fd_; o.fd_ = -1; }
        return *this;
    }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};
```

## Rule of 5 与 Rule of 0

**Rule of 5**：需要自定义任何一项特殊成员函数（析构、拷贝构造/赋值、移动构造/赋值），通常需要全部五项。

**Rule of 0**（首选）：让每个成员自己管理资源，类不需要自定义特殊成员函数：

```cpp
// Rule of 0 — 最佳实践
class Connection {
    std::string host_;
    std::unique_ptr<Socket> socket_;
    std::mutex send_mtx_;
public:
    explicit Connection(std::string host)
        : host_(std::move(host)), socket_(std::make_unique<Socket>()) {}
    // 不需要声明任何特殊成员函数
};

// Rule of 5 — 需要手动管理资源时
class Buffer {
    char* data_; std::size_t size_;
public:
    Buffer(std::size_t n) : data_(new char[n]), size_(n) {}
    ~Buffer() { delete[] data_; }
    Buffer(const Buffer& o) : data_(new char[o.size_]), size_(o.size_) {
        std::memcpy(data_, o.data_, size_);
    }
    Buffer& operator=(const Buffer& o) {
        if (this != &o) {
            // ✅ 先分配新缓冲区，再释放旧缓冲区——保证异常安全
            // 若 new 抛异常，当前对象仍保持原状（strong exception guarantee）
            char* new_data = new char[o.size_];
            std::memcpy(new_data, o.data_, o.size_);
            delete[] data_;
            data_ = new_data;
            size_ = o.size_;
        }
        return *this;
    }
    Buffer(Buffer&& o) noexcept : data_(o.data_), size_(o.size_) {
        o.data_ = nullptr; o.size_ = 0;
    }
    Buffer& operator=(Buffer&& o) noexcept {
        delete[] data_; data_ = o.data_; size_ = o.size_;
        o.data_ = nullptr; o.size_ = 0; return *this;
    }
};
```

## 智能指针

```cpp
// unique_ptr: 独占所有权，零运行时开销
auto up = std::make_unique<int>(42);
auto file = std::unique_ptr<FILE, decltype(&fclose)>(
    fopen("out.txt", "w"), &fclose);

// shared_ptr: 共享所有权（原子引用计数，有开销）
auto sp1 = std::make_shared<Widget>(); // 单次分配（控制块+对象）
auto sp2 = sp1;  // 引用计数 +1

// ⚠️ 陷阱：两个独立 shared_ptr 管理同一个裸指针 → 双重释放
// 正确做法：使用 enable_shared_from_this

// weak_ptr: 不增加引用计数的观察者
std::weak_ptr<Widget> wp = sp1;
if (auto locked = wp.lock()) { /* 对象存活 */ }
```

## lock_guard 与 unique_lock

```cpp
// lock_guard: 最简单的 RAII 锁，无额外开销
void safe() {
    std::lock_guard lock(mtx);
} // 自动解锁

// unique_lock: 延迟加锁、手动 lock/unlock、配合 condition_variable
void complex() {
    std::unique_lock lock(mtx, std::defer_lock);
    lock.lock();   // 临界区
    lock.unlock(); // 继续做不需要锁的工作
}

// C++17: scoped_lock — 同时锁多个互斥量，无死锁
void transfer(Account& from, Account& to, int amount) {
    std::scoped_lock lock(from.mtx, to.mtx);
    from.balance -= amount;
    to.balance += amount;
}
```

## Scope Guard 模式

```cpp
template <typename F>
class scope_guard {
    F func_; bool active_ = true;
public:
    explicit scope_guard(F f) : func_(std::move(f)) {}
    // ⚠️ 析构函数中调用 func_()：若 func_ 抛异常，会触发 std::terminate
    // 清理函数必须保证不抛异常（noexcept 或内部 try-catch）
    ~scope_guard() { if (active_) func_(); }
    void dismiss() noexcept { active_ = false; }
    scope_guard(const scope_guard&) = delete;
    scope_guard(scope_guard&& o) noexcept
        : func_(std::move(o.func_)), active_(o.active_) { o.active_ = false; }
};
template <typename F> scope_guard(F) -> scope_guard<F>;
```

## 现代 C++ 实践

```cpp
// 1. 优先值语义 — 自动管理生命周期
void process() {
    std::vector<int> data = load();
    save(transform(data));
}

// 2. 所有权转移通过移动语义
std::unique_ptr<Connection> connect() {
    auto conn = std::make_unique<Connection>(host);
    conn->authenticate();
    return conn;
}
```

## RAII 的工作原理：生命周期时序

```
构造阶段：
  1. 构造函数获取资源（fd = open(...)）
  2. 若获取失败，构造函数抛异常 → 对象从未"存在"，不调用析构函数

正常使用：
  3. 对象在作用域内被使用

析构阶段（正常退出或栈展开）：
  4. 编译器保证按声明的**逆序**调用局部对象的析构函数
  5. 析构函数释放资源（close(fd)）
  6. 析构函数不会被调用两次（标准保证）

关键：栈展开期间，编译器会为每个已构造的局部对象调用析构函数，
      因此即使中间某步抛异常，之前获取的资源也会被正确释放。
```

## 常见误区

1. **RAII 不等于"只管内存"**。文件描述符、互斥锁、网络连接、数据库事务都可以用 RAII 管理。
2. **析构函数不能抛异常**。析构期间抛异常会导致 `std::terminate`（C++11 起析构函数默认 `noexcept`）。
3. **`std::move` 不会自动释放资源**。`std::move` 只是将左值转为右值引用，实际资源转移由移动构造/赋值完成。
4. **`shared_ptr` 循环引用不会自动释放**。必须用 `weak_ptr` 打破循环。
5. **`unique_ptr` 带自定义删除器时不是零开销**。`unique_ptr<FILE, decltype(&fclose)>` 比默认删除器多占一个指针大小，且析构时多一次间接调用。

## 延伸阅读

- [异常安全等级](/topics/cpp-jargon/exception-safety) — basic/strong/nothrow 保证
- [智能指针实现原理](/libraries/llvm/function-shared-ptr) — libc++ `shared_ptr` 控制块设计
- [Rule of Five 术语解释](/topics/cpp-jargon/special-members) — 特殊成员函数详解
- C++ Core Guidelines [R.1–R.5](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r-resource-management) — 资源管理最佳实践
