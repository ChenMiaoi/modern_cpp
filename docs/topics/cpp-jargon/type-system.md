# 类型系统术语

## Type Erasure（类型擦除）

将具体类型信息隐藏到统一接口后面——`std::function`、`std::any`、`std::shared_ptr` 都使用了类型擦除。

```cpp
// std::function 的简化实现思路：
template<typename Ret, typename... Args>
class function<Ret(Args...)> {
  // 每个具体 callable 类型都被擦除为统一接口
  struct concept_base {
    virtual Ret invoke(Args...) = 0;
    virtual ~concept_base() = 0;
  };

  template<typename F>
  struct model : concept_base {
    F f_;
    Ret invoke(Args... args) override { return f_(args...); }
  };

  concept_base* ptr_;  // 指向堆上的 model<F>
};

// 用户看到的类型：function<int(int)>
// 实际存储的类型：model<具体lambda类型>*
// 类型信息被"擦除"了
```

## Type Punning（类型双关）

通过不同的类型解释同一块内存。有安全和不安全两种：

```cpp
// 不安全——UB（违反 strict aliasing）
float f = 3.14f;
int i = *(int*)&f;  // UB!

// 安全——通过 union（C++ 中仍有争议，但 GCC/Clang 支持）
union { float f; int i; } u;
u.f = 3.14f;
int i2 = u.i;  // GCC/Clang 允许，标准立场不明确

// 最安全——C++20
int i3 = std::bit_cast<int>(f);  // 明确支持，无 UB
```

## Type Traits（类型特征）

编译期的类型查询和变换：

```cpp
// 查询
std::is_same_v<int, int>        // true
std::is_integral_v<double>      // false
std::is_trivially_copyable_v<std::string>  // false

// 变换
std::remove_const_t<const int>  // int
std::add_pointer_t<int>         // int*
std::decay_t<const int&>        // int（移除引用和 cv 限定符）
```

## Tag Dispatching（标签分发）

通过空类型标签在编译期选择最优实现路径：

```cpp
template<typename Iter>
void advance(Iter& it, int n, std::random_access_iterator_tag) {
  it += n;  // O(1)
}

template<typename Iter>
void advance(Iter& it, int n, std::input_iterator_tag) {
  while (n--) ++it;  // O(n)
}

template<typename Iter>
void advance(Iter& it, int n) {
  advance(it, n, typename std::iterator_traits<Iter>::iterator_category{});
  // 编译期根据迭代器类型选择最优路径
}
```

## Polymorphism（多态）

### Static Polymorphism（静态多态）

编译期确定的多态——CRTP、Concept、重载：

```cpp
template<typename T>
void process(T& obj) {
  obj.do_something();  // 编译期确定调用哪个 do_something
}
```

### Dynamic Polymorphism（动态多态）

运行时确定的多态——虚函数：

```cpp
void process(Base& obj) {
  obj.do_something();  // 运行时通过 vtable 分发
}
```

| 维度 | 静态多态 | 动态多态 |
|------|---------|---------|
| 绑定时机 | 编译期 | 运行时 |
| 开销 | 零（可内联） | vtable 间接调用 |
| 异构容器 | 困难 | 容易（`vector<Base*>`） |
| 代码膨胀 | 每个类型一份实例化 | 共享一份虚函数代码 |
