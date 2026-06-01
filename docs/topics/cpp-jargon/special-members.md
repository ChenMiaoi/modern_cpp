# 构造、析构与特殊成员

## Rule of Zero / Three / Five

### Rule of Zero（零法则）

如果类不管理资源，不要定义任何特殊成员函数——让编译器生成：

```cpp
struct Point {
  double x, y;
  // 编译器自动生成：拷贝构造、拷贝赋值、移动构造、移动赋值、析构
};
```

### Rule of Three（三法则，C++98）

如果定义了**析构**、**拷贝构造**、**拷贝赋值**中的任何一个，通常需要定义全部三个。

### Rule of Five（五法则，C++11）

C++11 增加了移动语义后，扩展为五个：析构 + 拷贝构造 + 拷贝赋值 + **移动构造** + **移动赋值**。

```cpp
class Buffer {
  size_t size_;
  char* data_;
public:
  ~Buffer() { delete[] data_; }                                    // 1. 析构
  Buffer(const Buffer& o) : size_(o.size_), data_(new char[o.size_]) // 2. 拷贝构造
    { std::copy(o.data_, o.data_+size_, data_); }
  Buffer& operator=(const Buffer& o) { ... }                        // 3. 拷贝赋值
  Buffer(Buffer&& o) noexcept : size_(o.size_), data_(o.data_)      // 4. 移动构造
    { o.size_ = 0; o.data_ = nullptr; }
  Buffer& operator=(Buffer&& o) noexcept { ... }                    // 5. 移动赋值
};
```

## Copy Elision（拷贝消除）

### RVO（Return Value Optimization）

```cpp
std::string make() {
  return std::string("hello");  // C++17: 直接在调用者位置构造，无拷贝
}
```

### NRVO（Named Return Value Optimization）

```cpp
std::string make() {
  std::string s = "hello";
  return s;  // 编译器可能直接在调用者位置构造 s（可选优化）
}
```

**C++17 的变化**：RVO 是**强制的**（guaranteed copy elision——prvalue 不创建临时对象）。NRVO 仍然是可选的优化。

## Trivially Copyable

如果一个类型可以安全地用 `memcpy` 复制（没有自定义拷贝构造函数、虚函数等），它是 trivially copyable 的。这是 `memcpy` 优化的前提：

```cpp
static_assert(std::is_trivially_copyable_v<int>);         // ✓
static_assert(std::is_trivially_copyable_v<std::unique_ptr<int>>);  // ✓
static_assert(!std::is_trivially_copyable_v<std::string>); // ✗ 有自定义析构
```

## Trivially Relocatable（提案中）

如果一个类型可以通过 `memcpy` 完成"移动+析构源对象"的操作，它是 trivially relocatable 的。libc++ 已经利用这个概念优化 vector 的 resize：

```cpp
// unique_ptr 是 trivially relocatable 的：
// memcpy(dst, src, sizeof(unique_ptr)) 等价于：
// new(dst) unique_ptr(std::move(*src)); src->~unique_ptr();
```

## Aggregate Initialization（聚合初始化）

聚合类型（没有用户提供的构造函数、没有私有成员、没有虚函数等）可以用花括号直接初始化：

```cpp
struct Point { double x; double y; };
Point p = {1.0, 2.0};  // 聚合初始化

// C++17: 聚合初始化可以推导模板参数
template<typename T, typename U>
struct Pair { T first; U second; };
Pair pr{42, 3.14};  // CTAD: Pair<int, double>
```

## noexcept 的重要性

移动操作必须标记 `noexcept`，否则标准库的很多移动优化不会生效：

```cpp
// std::vector::resize 中：
// 如果 T 的 move 构造函数是 noexcept 的 → 使用 move
// 否则 → 回退到 copy（保证强异常安全）

// 你的类型不标记 noexcept → vector resize 永远用 copy → 性能灾难
Buffer(Buffer&& o) noexcept;  // 必须标记！
```
