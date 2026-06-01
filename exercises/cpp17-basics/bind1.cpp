// cpplings: bind1
// Title: Structured Bindings
// Description: Use C++17 structured bindings to unpack pairs, tuples,
//   structs, arrays, and map entries. Also bind references for mutation.
//
// Instructions:
//   Replace each "// TODO:" block with working code using structured
//   bindings (auto [name, ...] = expr;). Delete the _todo_ guard line
//   when you're done with that section.
//
// Hint: auto [a, b] = pair; — works for any tuple-like or aggregate type.

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
    int _todo_ = "FILL IN"; (void)_todo_;
    // TODO: Use structured bindings to unpack the pair returned by get_name_age()
    //   auto [name, age] = get_name_age();

    // ASSERT_EQ(name, "Alice");
    // ASSERT_EQ(age, 30);
}

TEST("structured bindings — tuple") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // TODO: Use structured bindings to unpack the tuple returned by get_mixed()
    //   auto [i, d, s] = get_mixed();

    // ASSERT_EQ(i, 42);
    // ASSERT_EQ(d, 3.14);
    // ASSERT_EQ(s, "hello");
}

TEST("structured bindings — struct members") {
    int _todo_ = "FILL IN"; (void)_todo_;
    Point p{1.0, 2.0, 3.0};
    // TODO: Use structured bindings to decompose the struct
    //   auto [x, y, z] = p;

    // ASSERT_EQ(x, 1.0);
    // ASSERT_EQ(y, 2.0);
    // ASSERT_EQ(z, 3.0);
}

TEST("structured bindings — array") {
    int _todo_ = "FILL IN"; (void)_todo_;
    int arr[3] = {10, 20, 30};
    // TODO: Use structured bindings to decompose the array
    //   auto [a, b, c] = arr;

    // ASSERT_EQ(a, 10);
    // ASSERT_EQ(b, 20);
    // ASSERT_EQ(c, 30);
}

TEST("structured bindings — map iteration") {
    int _todo_ = "FILL IN"; (void)_todo_;
    std::map<std::string, int> scores = {
        {"Alice", 95}, {"Bob", 87}, {"Charlie", 92}
    };
    int total = 0;
    // TODO: Use structured bindings in a range-for to iterate the map
    //   for (const auto& [name, score] : scores) { total += score; }

    // ASSERT_EQ(total, 274);
}

TEST("structured bindings — reference binding") {
    int _todo_ = "FILL IN"; (void)_todo_;
    Point p{1.0, 2.0, 3.0};
    // TODO: Use reference bindings to mutate p in-place
    //   auto& [rx, ry, rz] = p;
    //   rx = 10.0; ry = 20.0; rz = 30.0;

    // ASSERT_EQ(p.x, 10.0);
    // ASSERT_EQ(p.y, 20.0);
    // ASSERT_EQ(p.z, 30.0);
}

CPPLINGS_MAIN
