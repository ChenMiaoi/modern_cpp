// Solution: Structured Bindings

#include "cpplings.h"
#include <tuple>
#include <map>
#include <string>
#include <utility>

struct Point {
    double x;
    double y;
    double z;
};

std::pair<std::string, int> get_name_age() {
    return {"Alice", 30};
}

std::tuple<int, double, std::string> get_mixed() {
    return {42, 3.14, "hello"};
}

TEST("structured bindings — pair") {
    auto [name, age] = get_name_age();

    ASSERT_EQ(name, "Alice");
    ASSERT_EQ(age, 30);
}

TEST("structured bindings — tuple") {
    auto [i, d, s] = get_mixed();

    ASSERT_EQ(i, 42);
    ASSERT_EQ(d, 3.14);
    ASSERT_EQ(s, "hello");
}

TEST("structured bindings — struct members") {
    Point p{1.0, 2.0, 3.0};
    auto [x, y, z] = p;

    ASSERT_EQ(x, 1.0);
    ASSERT_EQ(y, 2.0);
    ASSERT_EQ(z, 3.0);
}

TEST("structured bindings — array") {
    int arr[3] = {10, 20, 30};
    auto [a, b, c] = arr;

    ASSERT_EQ(a, 10);
    ASSERT_EQ(b, 20);
    ASSERT_EQ(c, 30);
}

TEST("structured bindings — map iteration") {
    std::map<std::string, int> scores = {
        {"Alice", 95}, {"Bob", 87}, {"Charlie", 92}
    };
    int total = 0;
    for (const auto& [name, score] : scores) { total += score; }

    ASSERT_EQ(total, 274);
}

TEST("structured bindings — reference binding") {
    Point p{1.0, 2.0, 3.0};
    auto& [rx, ry, rz] = p;
    rx = 10.0; ry = 20.0; rz = 30.0;

    ASSERT_EQ(p.x, 10.0);
    ASSERT_EQ(p.y, 20.0);
    ASSERT_EQ(p.z, 30.0);
}

CPPLINGS_MAIN
