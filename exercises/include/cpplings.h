#pragma once
// Cpplings — lightweight test harness for Modern C++ exercises
// Usage:
//   #include "cpplings.h"
//   TEST("description") { ASSERT_EQ(a, b); }
//   CPPLINGS_MAIN  // at the bottom of the file

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>
#include <functional>

#include <utility>
#include <tuple>
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#include <optional>
#include <variant>
#include <string_view>
#endif
namespace cpplings {
namespace detail {

struct TestCase {
    const char* name;
    std::function<void()> fn;
    bool passed;
};

inline std::vector<TestCase>& tests() {
    static std::vector<TestCase> t;
    return t;
}

inline int& failures() {
    static int f = 0;
    return f;
}

inline void report(const char* expr, const char* file, int line,
                   const char* /*lhs_s*/, const char* /*rhs_s*/,
                   const std::string& lhs_v, const std::string& rhs_v) {
    std::fprintf(stderr,
        "\033[1;31m  FAIL\033[0m %s:%d\n"
        "       %s\n"
        "         left: %s\n"
        "        right: %s\n",
        file, line, expr, lhs_v.c_str(), rhs_v.c_str());
}

// C++11-compatible to_str via SFINAE overloads

template <typename T>
typename std::enable_if<std::is_same<T, bool>::value, std::string>::type
to_str(const T& v) { return v ? "true" : "false"; }

template <typename T>
typename std::enable_if<std::is_same<T, char>::value, std::string>::type
to_str(const T& v) { return std::string(1, v); }

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value
    && !std::is_same<T, bool>::value && !std::is_same<T, char>::value, std::string>::type
to_str(const T& v) { std::ostringstream os; os << v; return os.str(); }

template <typename T>
typename std::enable_if<!std::is_arithmetic<T>::value
    && std::is_convertible<T, std::string>::value, std::string>::type
to_str(const T& v) { return static_cast<std::string>(v); }

template <typename T>
typename std::enable_if<!std::is_arithmetic<T>::value
    && !std::is_convertible<T, std::string>::value, std::string>::type
to_str(const T&) { return "(value)"; }

template <typename A, typename B>
std::string to_str(const std::pair<A, B>& p) {
    return "{" + to_str(p.first) + ", " + to_str(p.second) + "}";
}

template <typename T>
std::string to_str(const std::vector<T>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s += ", ";
        s += to_str(v[i]);
    }
    s += "]";
    return s;
}

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)

template <typename T>
std::string to_str(const std::optional<T>& opt) {
    if (opt.has_value()) return "*" + to_str(*opt);
    return "nullopt";
}

inline std::string to_str(std::string_view sv) {
    return std::string(sv);
}

struct variant_to_str_visitor {
    template <typename T>
    std::string operator()(const T& v) const { return to_str(v); }
};

template <typename... Ts>
std::string to_str(const std::variant<Ts...>& var) {
    return "variant[" + std::to_string(var.index()) + "]: "
         + std::visit(variant_to_str_visitor{}, var);
}

#endif // C++17

} // namespace detail
} // namespace cpplings

// --- Test registration ---

#define CPPLINGS_CONCAT_(a, b) a##b
#define CPPLINGS_CONCAT(a, b) CPPLINGS_CONCAT_(a, b)

#define TEST(name)                                                          \
    static void CPPLINGS_CONCAT(_test_fn_, __LINE__)();                     \
    namespace { struct CPPLINGS_CONCAT(_reg_, __LINE__) {                   \
        CPPLINGS_CONCAT(_reg_, __LINE__)() {                                \
            ::cpplings::detail::tests().push_back(                          \
                {name, CPPLINGS_CONCAT(_test_fn_, __LINE__), false});       \
        }                                                                   \
    } CPPLINGS_CONCAT(_inst_, __LINE__); }                                  \
    static void CPPLINGS_CONCAT(_test_fn_, __LINE__)()

// --- Assertion macros ---

#define ASSERT_EQ(lhs, rhs) do {                                            \
    auto _lv = (lhs); auto _rv = (rhs);                                    \
    bool _ok = (_lv == _rv);                                                \
    if (!_ok) {                                                             \
        ::cpplings::detail::report(#lhs " == " #rhs, __FILE__, __LINE__,    \
            #lhs, #rhs,                                                    \
            ::cpplings::detail::to_str(_lv),                               \
            ::cpplings::detail::to_str(_rv));                              \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_TRUE(expr) do {                                              \
    if (!(expr)) {                                                          \
        std::fprintf(stderr, "\033[1;31m  FAIL\033[0m %s:%d  %s\n",        \
            __FILE__, __LINE__, #expr);                                     \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_THROWS(expr, ExType) do {                                    \
    bool _threw = false;                                                    \
    try { expr; } catch (const ExType&) { _threw = true; }                  \
    catch (...) {}                                                          \
    if (!_threw) {                                                          \
        std::fprintf(stderr, "\033[1;31m  FAIL\033[0m %s:%d  "             \
            "expected " #ExType " from " #expr "\n", __FILE__, __LINE__);   \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_NE(lhs, rhs) do {                                            \
    auto _lv = (lhs); auto _rv = (rhs);                                    \
    bool _ok = (_lv != _rv);                                                \
    if (!_ok) {                                                             \
        ::cpplings::detail::report(#lhs " != " #rhs, __FILE__, __LINE__,    \
            #lhs, #rhs,                                                    \
            ::cpplings::detail::to_str(_lv),                               \
            ::cpplings::detail::to_str(_rv));                              \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_LT(lhs, rhs) do {                                            \
    auto _lv = (lhs); auto _rv = (rhs);                                    \
    if (!(_lv < _rv)) {                                                     \
        ::cpplings::detail::report(#lhs " < " #rhs, __FILE__, __LINE__,    \
            #lhs, #rhs, ::cpplings::detail::to_str(_lv),                   \
            ::cpplings::detail::to_str(_rv));                               \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_LE(lhs, rhs) do {                                            \
    auto _lv = (lhs); auto _rv = (rhs);                                    \
    if (!(_lv <= _rv)) {                                                    \
        ::cpplings::detail::report(#lhs " <= " #rhs, __FILE__, __LINE__,   \
            #lhs, #rhs, ::cpplings::detail::to_str(_lv),                   \
            ::cpplings::detail::to_str(_rv));                               \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_GT(lhs, rhs) do {                                            \
    auto _lv = (lhs); auto _rv = (rhs);                                    \
    if (!(_lv > _rv)) {                                                     \
        ::cpplings::detail::report(#lhs " > " #rhs, __FILE__, __LINE__,    \
            #lhs, #rhs, ::cpplings::detail::to_str(_lv),                   \
            ::cpplings::detail::to_str(_rv));                               \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_GE(lhs, rhs) do {                                            \
    auto _lv = (lhs); auto _rv = (rhs);                                    \
    if (!(_lv >= _rv)) {                                                    \
        ::cpplings::detail::report(#lhs " >= " #rhs, __FILE__, __LINE__,   \
            #lhs, #rhs, ::cpplings::detail::to_str(_lv),                   \
            ::cpplings::detail::to_str(_rv));                               \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_NEAR(lhs, rhs, eps) do {                                     \
    auto _lv = (lhs); auto _rv = (rhs); auto _ep = (eps);                  \
    auto _diff = (_lv > _rv) ? (_lv - _rv) : (_rv - _lv);                  \
    if (_diff > _ep) {                                                      \
        std::fprintf(stderr, "\033[1;31m  FAIL\033[0m %s:%d\n"             \
            "       %s\n"                                                   \
            "         left: %s\n"                                           \
            "        right: %s\n"                                           \
            "     epsilon: %s\n",                                           \
            __FILE__, __LINE__, #lhs " ~= " #rhs,                          \
            ::cpplings::detail::to_str(_lv).c_str(),                        \
            ::cpplings::detail::to_str(_rv).c_str(),                        \
            ::cpplings::detail::to_str(_ep).c_str());                       \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define ASSERT_SAME_TYPE(T, U) do {                                         \
    static_assert(std::is_same<T, U>::value,                                \
        "type mismatch: " #T " != " #U);                                   \
} while(0)

#define ASSERT_NOT_SAME_TYPE(T, U) do {                                     \
    static_assert(!std::is_same<T, U>::value,                               \
        "types should differ: " #T " == " #U);                             \
} while(0)

#define ASSERT_NO_THROW(expr) do {                                          \
    bool _threw = false;                                                    \
    try { expr; } catch (...) { _threw = true; }                            \
    if (_threw) {                                                           \
        std::fprintf(stderr, "\033[1;31m  FAIL\033[0m %s:%d  "             \
            "unexpected exception from " #expr "\n", __FILE__, __LINE__);   \
        ::cpplings::detail::failures()++;                                  \
    }                                                                       \
} while(0)

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__)

// --- Type traits helpers (C++11) ---

namespace cpplings {
namespace typecheck {
    template <typename T, typename U>
    struct same : std::is_same<T, U> {};
} // namespace typecheck
} // namespace cpplings

// --- Main entry point ---

#define CPPLINGS_MAIN                                                       \
    int main() {                                                            \
        auto& _tests = ::cpplings::detail::tests();                        \
        int _pass = 0, _fail = 0;                                          \
        for (auto& t : _tests) {                                           \
            ::cpplings::detail::failures() = 0;                            \
            t.fn();                                                         \
            t.passed = (::cpplings::detail::failures() == 0);              \
            if (t.passed) {                                                 \
                std::fprintf(stderr, "\033[1;32m  PASS\033[0m %s\n",       \
                    t.name); _pass++;                                       \
            } else {                                                        \
                std::fprintf(stderr, "\033[1;31m  FAIL\033[0m %s\n",       \
                    t.name); _fail++;                                       \
            }                                                               \
        }                                                                   \
        std::fprintf(stderr, "\n  %d passed, %d failed\n", _pass, _fail);  \
        return _fail > 0 ? 1 : 0;                                          \
    }
