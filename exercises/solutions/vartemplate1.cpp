// Solution: 变量模板 (Variable Templates)

#include "cpplings.h"

template<typename T> constexpr T pi = T(3.1415926535897932385L);
template<typename T> constexpr T e = T(2.71828182845904523536L);

TEST("variable template pi with double") {
    ASSERT_TRUE(pi<double> > 3.14);
    ASSERT_TRUE(pi<double> < 3.15);
}

TEST("variable template pi with float") {
    ASSERT_TRUE(pi<float> > 3.14f);
    ASSERT_TRUE(pi<float> < 3.15f);
}

TEST("variable template e with double") {
    ASSERT_TRUE(e<double> > 2.71);
    ASSERT_TRUE(e<double> < 2.72);
}

TEST("variable template e with float") {
    ASSERT_TRUE(e<float> > 2.71f);
    ASSERT_TRUE(e<float> < 2.72f);
}

TEST("variable template used in expression") {
    double area = pi<double> * 2.0 * 2.0;
    ASSERT_TRUE(area > 12.56);
    ASSERT_TRUE(area < 12.57);
}

CPPLINGS_MAIN
