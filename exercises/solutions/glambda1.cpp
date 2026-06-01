// Solution: 泛型 Lambda (Generic Lambdas)

#include "cpplings.h"

TEST("generic add with int") {
    auto add = [](auto a, auto b) {
        return a + b;
    };
    ASSERT_EQ(add(3, 4), 7);
    ASSERT_EQ(add(10, 20), 30);
}

TEST("generic add with double") {
    auto add = [](auto a, auto b) {
        return a + b;
    };
    ASSERT_TRUE(add(1.5, 2.5) == 4.0);
    ASSERT_TRUE(add(0.1, 0.2) > 0.29);
}

TEST("generic multiply") {
    auto multiply = [](auto a, auto b) {
        return a * b;
    };
    ASSERT_EQ(multiply(3, 4), 12);
    ASSERT_EQ(multiply(2.5, 4.0), 10.0);
    ASSERT_EQ(multiply(6, 7), 42);
}

TEST("generic compare") {
    auto equal = [](auto a, auto b) {
        return a == b;
    };
    ASSERT_TRUE(equal(42, 42));
    ASSERT_FALSE(equal(42, 43));
    ASSERT_TRUE(equal(3.14, 3.14));
    ASSERT_FALSE(equal(3.14, 2.71));
}

CPPLINGS_MAIN
