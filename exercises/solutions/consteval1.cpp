// Solution — consteval1: consteval 与 constinit
#include "cpplings.h"
#include <type_traits>

consteval int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}

TEST("consteval factorial") {
    constexpr int f5 = factorial(5);
    ASSERT_EQ(f5, 120);
    constexpr int f0 = factorial(0);
    ASSERT_EQ(f0, 1);
    constexpr int f10 = factorial(10);
    ASSERT_EQ(f10, 3628800);
}

consteval bool is_prime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; ++i)
        if (n % i == 0) return false;
    return true;
}

TEST("consteval is_prime") {
    static_assert(is_prime(2));
    static_assert(is_prime(17));
    static_assert(!is_prime(1));
    static_assert(!is_prime(15));
    ASSERT_TRUE(is_prime(13));
    ASSERT_FALSE(is_prime(100));
}

constinit int global_counter = 0;
constinit static const char* prefix = "hello";

TEST("constinit variable") {
    constinit int local_init = 42;
    ASSERT_EQ(local_init, 42);
    local_init = 100;
    ASSERT_EQ(local_init, 100);
}

consteval int square(int n) { return n * n; }

TEST("consteval vs constexpr") {
    constexpr int s = square(7);
    ASSERT_EQ(s, 49);
    static_assert(square(5) == 25);
}

CPPLINGS_MAIN
