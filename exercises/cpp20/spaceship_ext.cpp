// cpplings: spaceship_ext
// Title: <=> 运算符扩展 — 字符串与容器比较
// Description: Extend the spaceship operator to work with std::string,
//   std::vector, and custom types with weak_ordering. Build on the
//   basics from spaceship1 with more complex comparison scenarios.
//
// Instructions:
//   1. Implement a Name struct with <=> comparing lowercase versions.
//   2. Implement a ScoreBoard that compares by total score using <=>.
//   3. Use <=> to compare vectors of ints lexicographically.
//   4. Use <=> to compare strings (standard behavior).
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: std::string already supports <=> (returns strong_ordering).
//       std::vector <=> does lexicographic comparison.

#include "cpplings.h"
#include <compare>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <numeric>

// TODO: Implement Name with case-insensitive <=>.
//   struct Name {
//       std::string value;
//       std::weak_ordering operator<=>(const Name& other) const {
//           // compare character-by-character using std::tolower
//           auto a = value, b = other.value;
//           for (auto& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
//           for (auto& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
//           return a <=> b;
//       }
//       bool operator==(const Name& other) const { return (*this <=> other) == 0; }
//   };

TEST("Name — case-insensitive spaceship") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Name a{"Alice"};
    // Name b{"alice"};
    // Name c{"Bob"};
    // ASSERT_TRUE(a == b);
    // ASSERT_TRUE(a < c);
    // ASSERT_FALSE(c < a);
}

// TODO: Implement ScoreBoard with total-based comparison.
//   struct ScoreBoard {
//       std::string team;
//       std::vector<int> scores;
//       int total() const { return std::accumulate(scores.begin(), scores.end(), 0); }
//       std::strong_ordering operator<=>(const ScoreBoard& other) const {
//           return total() <=> other.total();
//       }
//       bool operator==(const ScoreBoard& other) const { return (*this <=> other) == 0; }
//   };

TEST("ScoreBoard — compare by total") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ScoreBoard a{"Team A", {10, 20, 30}};
    // ScoreBoard b{"Team B", {5, 15, 25}};
    // ScoreBoard c{"Team C", {10, 20, 30}};
    // ASSERT_TRUE(b < a);
    // ASSERT_TRUE(a == c);
    // ASSERT_FALSE(a < b);
}

// TODO: Compare vectors lexicographically using <=>.
//   std::vector<int> a = {1, 2, 3};
//   std::vector<int> b = {1, 2, 4};
//   auto result = (a <=> b);
//   result is < 0

TEST("vector — lexicographic spaceship") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::vector<int> a = {1, 2, 3};
    // std::vector<int> b = {1, 2, 4};
    // std::vector<int> c = {1, 2};
    // auto r1 = (a <=> b);
    // ASSERT_TRUE(r1 < 0);
    // auto r2 = (a <=> a);
    // ASSERT_TRUE(r2 == 0);
    // auto r3 = (c <=> a);
    // ASSERT_TRUE(r3 < 0);  // shorter prefix is less
}

// TODO: Use <=> for string comparison.
//   std::string supports <=> natively.

TEST("string — spaceship comparison") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::string a = "apple";
    // std::string b = "banana";
    // std::string c = "apple";
    // auto r1 = (a <=> b);
    // ASSERT_TRUE(r1 < 0);
    // auto r2 = (a <=> c);
    // ASSERT_TRUE(r2 == 0);
    // auto r3 = (b <=> a);
    // ASSERT_TRUE(r3 > 0);
}

CPPLINGS_MAIN
