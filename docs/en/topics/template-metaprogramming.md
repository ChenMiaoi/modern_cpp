---
title: Template Metaprogramming
topic: topics
feature: template-metaprogramming
status_checked_at: 2026-06-01
standard: N/A
---

# Template Metaprogramming

## The Origins of TMP

In 1994, Erwin Unruh presented a program to the C++ Standards Committee that would not compile successfully — but the compiler error messages contained a sequence of prime numbers. This demonstrated that the C++ template system is Turing complete — it is effectively a functional language executed inside the compiler.

```cpp
template<int p, int i>
struct is_prime {
    enum { prim = (p == 2) || ((p % i) && is_prime<p, i - 1>::prim) };
};
template<int p>
struct is_prime<p, 1> { enum { prim = 1 }; };
```

## type_traits Basics

```cpp
static_assert(std::is_integral_v<int>);
static_assert(std::is_pointer_v<int*>);
using no_ref = std::remove_reference_t<int&>;    // int
static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
// C++20: static_assert(std::movable<std::string>);
//        static_assert(std::three_way_comparable<double>);
```

## SFINAE

SFINAE (Substitution Failure Is Not An Error): when template argument substitution fails, that overload is silently discarded.

```cpp
// Detect whether a type has a size() member
template <typename T, typename = void>
struct has_size : std::false_type {};
template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

static_assert(has_size<std::vector<int>>::value);
static_assert(!has_size<int>::value);
```

## enable_if and void_t

```cpp
// enable_if: enable an overload when the condition is true
// ⚠️ Note: this example only demonstrates the SFINAE constraint mechanism;
// overflow detection only covers signed positive overflow scenarios.
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
safe_add(T a, T b) {
    if (a > 0 && b > std::numeric_limits<T>::max() - a)
        throw std::overflow_error("overflow");
    return a + b;
}

// void_t: map expression validity to a type constraint
template <typename T, typename = void>
struct is_iterable : std::false_type {};
template <typename T>
struct is_iterable<T, std::void_t<
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().end())>> : std::true_type {};
```

## constexpr if (C++17)

`if constexpr` evaluates a condition at compile time and discards the branch that does not match:

```cpp
template <typename T>
auto process(T value) {
    if constexpr (std::is_integral_v<T>) return value * 2;
    else if constexpr (std::is_floating_point_v<T>) return value + 0.5;
    else static_assert(always_false<T>::value, "unsupported type");
}
template <typename> struct always_false : std::false_type {};

// Key advantage: both branches do not need to be valid simultaneously
// (the two branches of process do not both need to compile — the non-matching branch is discarded)
```

```cpp
// C++20: if constexpr + requires expression (combined usage)
template <typename T>
void serialize(const T& obj) {
    if constexpr (requires { obj.serialize(); }) obj.serialize();
    else { /* default serialization */ }
}
```
## Concepts (C++20)

Concepts elevate constraints to first-class citizens, greatly improving the readability of template code and the quality of error messages:

```cpp
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Sortable = std::ranges::random_access_range<T>
    && std::sortable<std::ranges::iterator_t<T>>;

// Constrained function
template <Sortable Range>
void sort_and_print(Range& r) { std::ranges::sort(r); }
// Note: Sortable requires random_access_range, so std::list does not satisfy this constraint.
// This is consistent with the real signature of std::ranges::sort.

// Abbreviated syntax
void process(std::integral auto val) { /* ... */ };

// requires clause
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};
template <typename T> requires Printable<T> && std::movable<T>
void log(T&& value) { std::cout << value << '\n'; }
```

## Compile-Time Dispatch Patterns

```cpp
// if constexpr (preferred)
template <typename T> std::string type_name() {
    if constexpr (std::is_same_v<T, int>) return "int";
    else if constexpr (std::is_same_v<T, double>) return "double";
    else return "unknown";
}

// Template specialization
template <typename T> struct Serializer;
template <> struct Serializer<int> {
    static void write(std::ostream& os, int v) {
        os.write(reinterpret_cast<const char*>(&v), 4);
    }
};

// Concepts + overloading (C++20)
std::string describe(std::integral auto v) { return "integer"; }
std::string describe(std::floating_point auto v) { return "float"; }
std::string describe(const std::ranges::range auto&) { return "range"; }
```

## Policy-Based Design (Alexandrescu)

The policy-based design pattern composes behavior through template parameters, building flexible components with zero overhead at compile time:

```cpp
struct SingleThreaded { struct Lock { explicit Lock(...) {} }; };
template <typename T>
struct CreateNew { static T* Create() { return new T; } };

template <typename T, typename ThreadingModel = SingleThreaded,
          template<typename> class CreationPolicy = CreateNew>
class SmartPtr {
    T* pointee_;
public:
    SmartPtr() : pointee_(CreationPolicy<T>::Create()) {}
    T* operator->() {
        typename ThreadingModel::Lock guard(/* ... */);
        return pointee_;
    }
};
```

## C++26 Reflection

C++26 introduces compile-time reflection (P2996), allowing program structure to be inspected and manipulated at compile time:
```cpp
// Generate serialization code at compile time (note: ^T is the reflection operator, [:e:] is the splice operator)
template <typename T>
std::string to_json(const T& obj) {
    std::string result = "{";
    bool first = true;
    // nonstatic_data_members_of iterates only over data members, excluding member functions, etc.
    template for (constexpr auto mem : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) result += ", "; first = false;
        result += "\"" + std::string(std::meta::name_of(mem)) + "\"";
        result += ": " + serialize(obj.[:mem:]);
    }
    return result + "}";
}
```
