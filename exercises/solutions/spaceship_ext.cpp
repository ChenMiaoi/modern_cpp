// Solution — spaceship_ext: <=> 运算符扩展
#include "cpplings.h"
#include <compare>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <numeric>

struct Name {
    std::string value;

    std::weak_ordering operator<=>(const Name& other) const {
        auto a = value, b = other.value;
        for (auto& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return a <=> b;
    }

    bool operator==(const Name& other) const { return (*this <=> other) == 0; }
};

TEST("Name — case-insensitive spaceship") {
    Name a{"Alice"};
    Name b{"alice"};
    Name c{"Bob"};
    ASSERT_TRUE(a == b);
    ASSERT_TRUE(a < c);
    ASSERT_FALSE(c < a);
}

struct ScoreBoard {
    std::string team;
    std::vector<int> scores;

    int total() const { return std::accumulate(scores.begin(), scores.end(), 0); }

    std::strong_ordering operator<=>(const ScoreBoard& other) const {
        return total() <=> other.total();
    }

    bool operator==(const ScoreBoard& other) const { return (*this <=> other) == 0; }
};

TEST("ScoreBoard — compare by total") {
    ScoreBoard a{"Team A", {10, 20, 30}};
    ScoreBoard b{"Team B", {5, 15, 25}};
    ScoreBoard c{"Team C", {10, 20, 30}};
    ASSERT_TRUE(b < a);
    ASSERT_TRUE(a == c);
    ASSERT_FALSE(a < b);
}

TEST("vector — lexicographic spaceship") {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b = {1, 2, 4};
    std::vector<int> c = {1, 2};
    auto r1 = (a <=> b);
    ASSERT_TRUE(r1 < 0);
    auto r2 = (a <=> a);
    ASSERT_TRUE(r2 == 0);
    auto r3 = (c <=> a);
    ASSERT_TRUE(r3 < 0);
}

TEST("string — spaceship comparison") {
    std::string a = "apple";
    std::string b = "banana";
    std::string c = "apple";
    auto r1 = (a <=> b);
    ASSERT_TRUE(r1 < 0);
    auto r2 = (a <=> c);
    ASSERT_TRUE(r2 == 0);
    auto r3 = (b <=> a);
    ASSERT_TRUE(r3 > 0);
}

CPPLINGS_MAIN
