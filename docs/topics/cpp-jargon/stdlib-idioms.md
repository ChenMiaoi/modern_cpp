# 标准库惯用语

## CPO（Customization Point Object）

标准库中通过 ADL 发现用户自定义实现的函数对象：

```cpp
// std::ranges::begin 是一个 CPO
// 调用 ranges::begin(v) 时：
// 1. 如果 v 有成员 begin() → 调用 v.begin()
// 2. 否则通过 ADL 找到 begin(v)
// 3. 否则回退到默认实现

namespace MyLib {
  struct Container { ... };
  auto begin(Container& c) { return c.data(); }  // ADL 发现
}

MyLib::Container c;
auto it = std::ranges::begin(c);  // 通过 ADL 找到 MyLib::begin
```

## Niebloid

CPO 的一种实现形式——全局函数对象，通过 ADL 机制工作但本身不是函数：

```cpp
// ranges::begin 是一个 niebloid（以 Eric Niebler 命名）
namespace std::ranges {
  inline constexpr __begin_fn begin{};  // 全局 constexpr 函数对象
}
```

## Tag Invoke（C++20 Ranges 的自定义机制）

```cpp
// 用户类型自定义 ranges::begin 的推荐方式
template<typename T>
auto tag_invoke(ranges::begin_t, MyContainer<T>& c) {
  return my_iterator<T>(c.data());
}
```

## Range / View / Pipe Operator

- **Range**：有 `begin()` 和 `end()` 的任何东西
- **View**：轻量的 range 适配器——O(1) 拷贝、O(1) 默认构造、非拥有
- **Pipe Operator**：`|` 运算符将 range 传递给 view 适配器

详见 [range-v3 管道运算符章节](/libraries/range-v3/pipe-operator)。

## Sentinel（哨兵）

`end()` 可以不是迭代器，而是一个"哨兵"——与迭代器可比较的任意类型：

```cpp
// null-terminated string 的哨兵
struct null_sentinel {
  bool operator==(const char* p) const { return *p == '\0'; }
};

// 使用
const char* s = "hello";
auto range = ranges::subrange(s, null_sentinel{});
```

## PMR（Polymorphic Memory Resource，C++17）

运行时多态的内存分配器——通过虚函数分发而非模板参数：

```cpp
std::pmr::monotonic_buffer_resource pool(buf, sizeof(buf));
std::pmr::vector<int> v(&pool);  // 使用 pool 分配内存
v.push_back(42);                  // 从 pool 分配，不走全局 new
```

## Allocator Model

标准库分配器是类型绑定的——`vector<int, MyAlloc>` 和 `vector<double, MyAlloc>` 是不同的类型。这使得运行时切换分配策略困难。PMR 和 EASTL 的非模板分配器是对此问题的两种解决方案。

## Smart Pointer 惯用语

```cpp
// unique_ptr: 独占所有权，零开销（8 字节）
auto p = std::make_unique<Widget>(args...);

// shared_ptr: 共享所有权，引用计数（16 字节）
auto p = std::make_shared<Widget>(args...);  // 一次分配

// weak_ptr: 不拥有对象，可以检查对象是否存活
std::weak_ptr<Widget> wp = sp;
if (auto locked = wp.lock()) { /* 对象还活着 */ }
```

## RAII Handle

标准库中大量使用 RAII 包装操作系统资源：

```cpp
std::lock_guard<std::mutex> lk(mtx);    // 互斥锁
std::unique_ptr<File> fp(open(...));     // 文件句柄
std::jthread worker(do_work);            // 线程
```
