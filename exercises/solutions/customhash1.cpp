// Solution: 自定义哈希
// std::unordered_map 默认只能哈希标准类型。
// 要让自定义类型作为键，需要特化 std::hash 或提供自定义哈希器。

#include "cpplings.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <cctype>
#include <cstddef>

struct Point {
    int x;
    int y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// 特化 std::hash<Point>
namespace std {
template <>
struct hash<Point> {
    std::size_t operator()(const Point& p) const {
        std::size_t h1 = std::hash<int>()(p.x);
        std::size_t h2 = std::hash<int>()(p.y);
        return h1 ^ (h2 << 1);
    }
};
} // namespace std

// 自定义哈希器 — 大小写不敏感
struct CaseInsensitiveHash {
    std::size_t operator()(const std::string& s) const {
        std::size_t hash = 0;
        for (char c : s) {
            hash = hash * 31 + std::tolower(static_cast<unsigned char>(c));
        }
        return hash;
    }
};

// 自定义相等谓词 — 大小写不敏感
struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }
};

// --- Tests ---

TEST("Point 哈希可用 unordered_map") {
    std::unordered_map<Point, std::string> locations;

    locations[Point{0, 0}] = "origin";
    locations[Point{1, 2}] = "point_a";
    locations[Point{3, 4}] = "point_b";

    ASSERT_EQ(locations.size(), 3u);
    Point origin{0, 0};
    Point pa{1, 2};
    ASSERT_EQ(locations[origin], std::string("origin"));
    ASSERT_EQ(locations[pa], std::string("point_a"));
}

TEST("Point find 和 insert") {
    std::unordered_map<Point, int> distances;
    Point p1{10, 20};
    Point p2{30, 40};
    Point p3{10, 20};

    distances[p1] = 100;
    distances[p2] = 200;

    auto it = distances.find(p3);
    ASSERT_TRUE(it != distances.end());
    ASSERT_EQ(it->second, 100);

    Point p4{99, 99};
    ASSERT_TRUE(distances.find(p4) == distances.end());
}

TEST("哈希组合 — 不同点产生不同哈希（大概率）") {
    std::hash<Point> hasher;
    Point p1{1, 2};
    Point p2{2, 1};
    Point p3{3, 4};

    ASSERT_TRUE(hasher(p1) != hasher(p3));
    ASSERT_TRUE(hasher(p1) != hasher(p2));
}

TEST("大小写不敏感的 unordered_map") {
    std::unordered_map<std::string, int,
                       CaseInsensitiveHash,
                       CaseInsensitiveEqual> scores;

    scores["Alice"] = 95;
    scores["Bob"] = 88;

    auto it1 = scores.find("alice");
    ASSERT_TRUE(it1 != scores.end());
    ASSERT_EQ(it1->second, 95);

    auto it2 = scores.find("BOB");
    ASSERT_TRUE(it2 != scores.end());
    ASSERT_EQ(it2->second, 88);

    auto it3 = scores.find("Charlie");
    ASSERT_TRUE(it3 == scores.end());
}

TEST("CaseInsensitiveHash 等价字符串") {
    CaseInsensitiveHash hasher;
    CaseInsensitiveEqual eq;

    std::string a = "Hello";
    std::string b = "HELLO";
    std::string c = "hello";

    ASSERT_TRUE(eq(a, b));
    ASSERT_TRUE(eq(a, c));
    ASSERT_EQ(hasher(a), hasher(b));
    ASSERT_EQ(hasher(a), hasher(c));
}

CPPLINGS_MAIN
