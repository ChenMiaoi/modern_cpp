// cpplings: consteval1
// Title: consteval 与 constinit
// Description: Use consteval to force compile-time evaluation and
//   constinit for constant initialization without requiring constexpr.
//
// Instructions:
//   1. Implement factorial() as a consteval function.
//   2. Implement is_prime() as a consteval function.
//   3. Declare a constinit variable with a non-trivial initializer.
//   4. Show that consteval functions cannot be called at runtime.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: consteval means every call MUST produce a compile-time constant.
//       constinit guarantees constant initialization (no dynamic init order fiasco).

#include "cpplings.h"
#include <type_traits>

// TODO: Implement factorial as consteval.
//   consteval int factorial(int n) {
//       int result = 1;
//       for (int i = 2; i <= n; ++i) result *= i;
//       return result;
//   }

TEST("consteval factorial") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // constexpr int f5 = factorial(5);
    // ASSERT_EQ(f5, 120);
    // constexpr int f0 = factorial(0);
    // ASSERT_EQ(f0, 1);
    // constexpr int f10 = factorial(10);
    // ASSERT_EQ(f10, 3628800);
}

// TODO: Implement is_prime as consteval.
//   consteval bool is_prime(int n) {
//       if (n < 2) return false;
//       for (int i = 2; i * i <= n; ++i)
//           if (n % i == 0) return false;
//       return true;
//   }

TEST("consteval is_prime") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // static_assert(is_prime(2));
    // static_assert(is_prime(17));
    // static_assert(!is_prime(1));
    // static_assert(!is_prime(15));
    // ASSERT_TRUE(is_prime(13));
    // ASSERT_FALSE(is_prime(100));
}

// TODO: Use constinit to declare a variable with constant initialization.
//   constinit int global_counter = 0;
//   constinit static const char* prefix = "hello";

TEST("constinit variable") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // constinit int local_init = 42;
    // ASSERT_EQ(local_init, 42);
    // local_init = 100;  // constinit allows mutation
    // ASSERT_EQ(local_init, 100);
}

// TODO: consteval vs constexpr difference.
//   constexpr can be called at runtime (result not forced to be compile-time).
//   consteval MUST be called at compile-time — every invocation is immediate.
//   Write a consteval function square(n) and verify.

TEST("consteval vs constexpr") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // consteval int square(int n) { return n * n; }
    // constexpr int s = square(7);    // OK: compile-time
    // ASSERT_EQ(s, 49);
    // static_assert(square(5) == 25); // OK: compile-time context
}

CPPLINGS_MAIN
