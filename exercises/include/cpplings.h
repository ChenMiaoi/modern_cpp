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
                   const char* lhs_s, const char* rhs_s,
                   const std::string& lhs_v, const std::string& rhs_v) {
    std::fprintf(stderr,
        "\033[1;31m  FAIL\033[0m %s:%d\n"
        "       %s\n"
        "         left: %s\n"
        "        right: %s\n",
        file, line, expr, lhs_v.c_str(), rhs_v.c_str());
}

template <typename T>
std::string to_str(const T& v) {
    if constexpr (std::is_same_v<T, bool>)
        return v ? "true" : "false";
    else if constexpr (std::is_same_v<T, char>)
        return std::string(1, v);
    else if constexpr (std::is_arithmetic_v<T>) {
        std::ostringstream os;
        os << v;
        return os.str();
    }
    else if constexpr (std::is_convertible_v<T, std::string>)
        return static_cast<std::string>(v);
    else {
        std::ostringstream os;
        os << "(value)";
        return os.str();
    }
}

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
