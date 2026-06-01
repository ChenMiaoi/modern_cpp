// Solution: Three-way Comparison (operator<=>)

#include "cpplings.h"
#include <compare>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// === Point: use defaulted <=> ===

struct Point {
    double x, y;

    auto operator<=>(const Point&) const = default;
    bool operator==(const Point&) const = default;
};

TEST("Point — defaulted equality") {
    Point a{1.0, 2.0};
    Point b{3.0, 4.0};
    Point c{1.0, 2.0};

    ASSERT_TRUE(a == c);
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
}

TEST("Point — defaulted ordering") {
    Point a{1.0, 2.0};
    Point b{3.0, 4.0};
    Point c{1.0, 5.0};

    ASSERT_TRUE(a < b);    // member-wise: x compared first
    auto r1 = a <=> b;
    ASSERT_TRUE(r1 < 0);

    auto r3 = a.x <=> c.x;
    ASSERT_TRUE(r3 == 0);  // same x
}

// === Version: custom lexicographic <=> ===

struct Version {
    int major, minor, patch;

    auto operator<=>(const Version& other) const {
        if (auto c = major <=> other.major; c != 0) return c;
        if (auto c = minor <=> other.minor; c != 0) return c;
        return patch <=> other.patch;
    }

    bool operator==(const Version&) const = default;
};

TEST("Version — custom comparison") {
    Version v1{1, 0, 0};
    Version v2{1, 0, 1};
    Version v3{2, 0, 0};

    ASSERT_TRUE(v1 < v2);
    ASSERT_TRUE(v2 < v3);
    ASSERT_TRUE(v1 < v3);
    ASSERT_FALSE(v2 > v3);
}

TEST("Version — sort") {
    std::vector<Version> versions = {
        {2, 0, 0}, {1, 5, 3}, {1, 0, 0}, {1, 5, 0}, {3, 0, 0}
    };
    std::sort(versions.begin(), versions.end());

    ASSERT_EQ(versions[0].major, 1);
    ASSERT_EQ(versions[0].minor, 0);
    ASSERT_EQ(versions[0].patch, 0);
    ASSERT_EQ(versions.back().major, 3);
}

TEST("Version — equality") {
    Version v1{1, 2, 3};
    Version v2{1, 2, 3};
    Version v3{1, 2, 4};

    ASSERT_TRUE(v1 == v2);
    ASSERT_FALSE(v1 == v3);
    ASSERT_TRUE(v1 != v3);
}

// === CIString: case-insensitive weak_ordering ===

struct CIString {
    std::string data;

    std::weak_ordering operator<=>(const CIString& other) const {
        std::size_t len = std::min(data.size(), other.data.size());
        for (std::size_t i = 0; i < len; ++i) {
            auto c = std::tolower(static_cast<unsigned char>(data[i]));
            auto o = std::tolower(static_cast<unsigned char>(other.data[i]));
            if (c < o) return std::weak_ordering::less;
            if (c > o) return std::weak_ordering::greater;
        }
        return data.size() <=> other.data.size();
    }

    bool operator==(const CIString& other) const {
        return (*this <=> other) == 0;
    }
};

TEST("CIString — case-insensitive comparison") {
    CIString a{"Hello"};
    CIString b{"hello"};
    CIString c{"World"};

    ASSERT_TRUE(a == b);   // same ignoring case
    ASSERT_TRUE(a < c);    // 'h' < 'w'
    ASSERT_FALSE(c < a);
}

CPPLINGS_MAIN
