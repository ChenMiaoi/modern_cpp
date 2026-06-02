---
title: "Compile-Time Computation"
topic: unknown
feature: compile-time-computation
standard: N/A
status_checked_at: 2026-06-02
---
# Compile-Time Computation

## Evolution of `constexpr`

Starting from C++11's restricted functions, each standard version has significantly expanded the scope of code that can execute at compile time.

### C++11: Single `return` Statement

```cpp
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
static_assert(factorial(5) == 120);
```

### C++14: Full Function Bodies

Local variables, loops, and `if`/`switch` are now allowed:

```cpp
constexpr int gcd(int a, int b) {
    while (b != 0) { int t = b; b = a % b; a = t; }
    return a;
}
constexpr auto sq = [](int x) { return x * x; };
```

### C++17: `if constexpr` and Compile-Time Container Operations

```cpp
template <typename T>
constexpr auto absolute(T val) {
    if constexpr (std::is_unsigned_v<T>) return val;
    else return val < 0 ? -val : val;
}

constexpr auto make_lookup_table() {
    std::array<int, 10> table{};
    for (int i = 0; i < 10; ++i) table[i] = i * i;
    return table;
}
constexpr auto sq_table = make_lookup_table();
```

### C++20: `consteval`, `constinit`, and Major Extensions

Dynamic allocation (must not leak during compile time), virtual functions, unions, and try-catch are now permitted:

```cpp
constexpr auto make_vector() {
    std::vector<int> v{1, 2, 3};
    v.push_back(4);
    return v;
}

struct Base { constexpr virtual int value() const = 0; };
struct Derived : Base { constexpr int value() const override { return 42; } };
constexpr int v = Derived{}.value();
```

### C++23: `constexpr` string/vector

C++20/23 allow the use of `std::string` and `std::vector` during constant evaluation — their construction, modification, and destruction can all execute at compile time. This moves compile-time computation from "restricted scalar operations" to "dynamic containers are available." However, note that compile-time memory allocation has limits, and dynamically allocated storage typically cannot survive beyond a single constant evaluation.

```cpp
constexpr auto sorted = [] {
    std::array data{5, 3, 1, 4, 2};
    std::ranges::sort(data);
    return data;
}();
```

## `consteval` and `constinit` (C++20)

```cpp
// consteval — must be evaluated at compile time
consteval unsigned fourcc(char a, char b, char c, char d) {
    return unsigned(a) | (unsigned(b) << 8) | (unsigned(c) << 16) | (unsigned(d) << 24);
}
constexpr auto jpeg = fourcc('J','F','I','F');

// constinit — guarantees compile-time initialization, avoids the static initialization order fiasco
constinit auto global_config = Config{};
```

## Compile-Time Reflection (C++26)

```cpp
consteval std::size_t enum_count(auto enum_type) {
    return std::meta::enumerators_of(enum_type).size();
}
enum class Color { Red, Green, Blue };
static_assert(enum_count(^^Color) == 3);

// Compile-time enum→string generation
constexpr std::string_view color_name(Color c) {
    template for (constexpr auto e : std::meta::enumerators_of(^^Color)) {
        if (c == [:e:]) return std::meta::name_of(e);
    }
    return "unknown";
}

// Traverse struct members at compile time to generate serialization
template <typename T>
std::string to_json(const T& obj) {
    std::string result = "{";
    bool first = true;
    // nonstatic_data_members_of only iterates over data members, excluding member functions, etc.
    template for (constexpr auto mem : std::meta::nonstatic_data_members_of(^^T)) {
        if (!first) result += ", "; first = false;
        result += "\"" + std::string(std::meta::name_of(mem)) + "\"";
        result += ": " + serialize(obj.[:mem:]);
    }
    return result + "}";
}
```

## Practical Scenarios

### Lookup Table Generation

```cpp
constexpr auto crc32_table = [] {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
        table[i] = crc;
    }
    return table;
}();
```

### Compile-Time String Parsing

```cpp
constexpr uint32_t parse_hex(std::string_view sv) {
    uint32_t result = 0;
    for (char c : sv) {
        result <<= 4;
        if (c >= '0' && c <= '9') result |= c - '0';
        else if (c >= 'a' && c <= 'f') result |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') result |= c - 'A' + 10;
    }
    return result;
}
constexpr auto color = parse_hex("FF8800");

// Compile-time format string validation
consteval bool valid_format(std::string_view fmt) {
    bool percent = false;
    for (char c : fmt) {
        if (c == '%') { percent = !percent; continue; }
        if (percent && c != 'd' && c != 's' && c != '%') return false;
        percent = false;
    }
    return !percent;
}
```

### Compile-Time Configuration Validation

```cpp
struct Config {
    static constexpr int max_conn = 100;
    static constexpr int timeout_ms = 5000;
};
consteval bool validate_config() {
    static_assert(Config::max_conn > 0);
    static_assert(Config::timeout_ms < 60000, "timeout too large");
    return true;
}
```

The core value of compile-time computation: **shift runtime errors to compile time, and shift runtime computation to compile time.** The former improves correctness; the latter improves performance. "constexpr wherever possible" has become the consensus in modern C++.
