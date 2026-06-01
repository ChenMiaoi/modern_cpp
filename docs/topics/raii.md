# RAII 与资源管理

## RAII 原则

RAII（Resource Acquisition Is Initialization）是 C++ 最重要的惯用法：**资源的生命周期绑定到对象的生命周期**——构造函数获取资源，析构函数释放资源。C++ 保证局部对象在离开作用域时析构，因此 RAII 消除了资源泄漏的可能性。

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
        if (this != &o) { delete[] data_; size_ = o.size_;
            data_ = new char[size_]; std::memcpy(data_, o.data_, size_); }
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
    ~scope_guard() { if (active_) func_(); }
    void dismiss() noexcept { active_ = false; }
    scope_guard(const scope_guard&) = delete;
    scope_guard(scope_guard&& o) noexcept
        : func_(std::move(o.func_)), active_(o.active_) { o.active_ = false; }
};
template <typename F> scope_guard(F) -> scope_guard<F>;


```

## 现代 C++ 实践

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
