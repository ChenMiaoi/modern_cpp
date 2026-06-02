---
title: "Boost 元编程"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost 元编程

## Hana：Monad 驱动的编译期编程

Hana 彻底重新思考了编译期编程。核心突破：**类型和值统一**。

```cpp
// MPL（旧）：类型列表是纯类型层面的构造
using types = boost::mpl::vector<int, char, double>;
using first = boost::mpl::at_c<types, 0>::type;  // 纯类型操作

// Hana（新）：type_c<int> 是一个值
constexpr auto t = hana::type_c<int>;       // 值
using T = typename decltype(t)::type;        // 类型提取
constexpr auto types = hana::tuple_t<int, char, double>;  // constexpr tuple
```

### hana::tuple：递归继承实现

```cpp
template<> struct tuple_impl<> {};  // 空基类
template<typename Head, typename ...Tail>
struct tuple_impl<Head, Tail...> : tuple_impl<Tail...> {
    Head head_;  // 当前元素
};

template<std::size_t N, typename Tuple>
constexpr auto& get(Tuple& t) {
    return static_cast<tuple_leaf<N, element_t<N, Tuple>>&>(t).head_;
}
```

### Tag Dispatching

Hana 的算法通过 tag dispatching 选择最优实现：

```cpp
template<> struct transform_impl<tuple_tag> {
    template<typename ...T, typename F>
    static constexpr auto apply(tuple<T...> const& xs, F const& f) {
        // pack expansion → O(1) 模板深度
        return tuple<decltype(f(std::declval<T>()))...>{
            f(hana::at_c<Is>(xs))...
        };
    }
};
```

### Monad 体系

`hana::tuple`、`hana::optional`、`hana::either` 都是 Monad：

```cpp
constexpr auto x = hana::just(42);           // wrap
constexpr auto y = hana::transform(x, f);    // fmap
constexpr auto flat = hana::flatten(nested); // join
```

---

## Mp11：现代 C++11 元编程

Mp11 是 David Abrahams 和 Peter Dimov 设计的现代元编程库，用 `using` 别名模板取代了 MPL 的元函数：

```cpp
// MPL 风格（旧）
using result = boost::mpl::transform<types, boost::mpl::add_pointer<boost::mpl::_1>>::type;

// Mp11 风格（新）
using result = mp_transform<std::add_pointer_t, types>;
```

Mp11 的核心：**类型列表是 `mp_list<T...>` 或 `std::tuple<T...>`**，算法是 `using` 别名模板。编译速度快于 MPL 数倍。

---

## PFR：编译期结构体反射

Boost.PFR（Precise and Flat Reflection）在 C++14/17/20 中实现编译期结构体字段遍历：

```cpp
struct Point { double x; double y; double z; };

// 遍历所有字段
boost::pfr::for_each_field(Point{1.0, 2.0, 3.0}, [](auto& field, auto idx) {
    std::cout << "field " << idx << ": " << field << "\n";
});

// 按索引访问
Point p{1.0, 2.0, 3.0};
auto& y = boost::pfr::get<1>(p);  // y = 2.0
```

PFR 不使用宏或侵入式标记——它利用聚合初始化的特性（C++17 结构化绑定 + `decltype` 推导）在编译期发现字段数量和类型。这在 C++26 反射标准化之前是最干净的方案。

---

## Describe：类型描述

Boost.Describe 提供基于宏的类型元数据注册：

```cpp
struct Point {
    int x;
    int y;
};

BOOST_DESCRIBE_STRUCT(Point, (), (x, y))

// 运行时遍历
boost::describe::for_each_member<Point>([](auto D) {
    std::cout << D.name() << ": " << D.pointer << "\n";
});
```

与 PFR 的区别：Describe 需要宏注册，但支持非聚合类型、继承和私有成员。PFR 零侵入但仅支持聚合类型。

---

## TypeTraits：类型特征

Boost.TypeTraits 是 `<type_traits>` 的前身——在 C++11 标准化之前提供类型查询和变换工具。现代项目应直接使用标准版本。
