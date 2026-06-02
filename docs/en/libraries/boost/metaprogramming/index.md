---
title: "Boost 元编程"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Metaprogramming

## Hana: Monad-Driven Compile-Time Programming

Hana completely reimagines compile-time programming. The core breakthrough: **types and values are unified**.

```cpp
// MPL (old): type lists are purely type-level constructs
using types = boost::mpl::vector<int, char, double>;
using first = boost::mpl::at_c<types, 0>::type;  // pure type operation

// Hana (new): type_c<int> is a value
constexpr auto t = hana::type_c<int>;       // value
using T = typename decltype(t)::type;        // type extraction
constexpr auto types = hana::tuple_t<int, char, double>;  // constexpr tuple
```

### hana::tuple: Recursive Inheritance Implementation

```cpp
template<> struct tuple_impl<> {};  // empty base class
template<typename Head, typename ...Tail>
struct tuple_impl<Head, Tail...> : tuple_impl<Tail...> {
    Head head_;  // current element
};

template<std::size_t N, typename Tuple>
constexpr auto& get(Tuple& t) {
    return static_cast<tuple_leaf<N, element_t<N, Tuple>>&>(t).head_;
}
```

### Tag Dispatching

Hana's algorithms select optimal implementations via tag dispatching:

```cpp
template<> struct transform_impl<tuple_tag> {
    template<typename ...T, typename F>
    static constexpr auto apply(tuple<T...> const& xs, F const& f) {
        // pack expansion → O(1) template depth
        return tuple<decltype(f(std::declval<T>()))...>{
            f(hana::at_c<Is>(xs))...
        };
    }
};
```

### Monad System

`hana::tuple`, `hana::optional`, and `hana::either` are all Monads:

```cpp
constexpr auto x = hana::just(42);           // wrap
constexpr auto y = hana::transform(x, f);    // fmap
constexpr auto flat = hana::flatten(nested); // join
```

---

## Mp11: Modern C++11 Metaprogramming

Mp11 is a modern metaprogramming library designed by David Abrahams and Peter Dimov, replacing MPL's metafunctions with `using` alias templates:

```cpp
// MPL style (old)
using result = boost::mpl::transform<types, boost::mpl::add_pointer<boost::mpl::_1>>::type;

// Mp11 style (new)
using result = mp_transform<std::add_pointer_t, types>;
```

The core of Mp11: **type lists are `mp_list<T...>` or `std::tuple<T...>`**, and algorithms are `using` alias templates. Compilation speed is several times faster than MPL.

---

## PFR: Compile-Time Struct Reflection

Boost.PFR (Precise and Flat Reflection) enables compile-time traversal of struct fields in C++14/17/20:

```cpp
struct Point { double x; double y; double z; };

// traverse all fields
boost::pfr::for_each_field(Point{1.0, 2.0, 3.0}, [](auto& field, auto idx) {
    std::cout << "field " << idx << ": " << field << "\n";
});

// access by index
Point p{1.0, 2.0, 3.0};
auto& y = boost::pfr::get<1>(p);  // y = 2.0
```

PFR uses no macros or intrusive markers — it leverages aggregate initialization features (C++17 structured bindings + `decltype` deduction) to discover field counts and types at compile time. This is the cleanest approach available before C++26 reflection standardization.

---

## Describe: Type Description

Boost.Describe provides macro-based type metadata registration:

```cpp
struct Point {
    int x;
    int y;
};

BOOST_DESCRIBE_STRUCT(Point, (), (x, y))

// runtime traversal
boost::describe::for_each_member<Point>([](auto D) {
    std::cout << D.name() << ": " << D.pointer << "\n";
});
```

The difference from PFR: Describe requires macro registration but supports non-aggregate types, inheritance, and private members. PFR is zero-intrusive but only supports aggregate types.

---

## TypeTraits: Type Traits

Boost.TypeTraits is the predecessor of `<type_traits>` — it provided type query and transformation utilities before C++11 standardization. Modern projects should use the standard library versions directly.
