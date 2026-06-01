// cpplings: optional1
// Title: std::optional
// Description: Use std::optional to represent values that may be absent.
//   Implement lookup and parsing functions that return optional, then use
//   has_value(), value_or(), and monadic operations.
//
// Instructions:
//   1. Implement find_user() — return std::optional<User> from a mock DB.
//   2. Implement parse_int() — return std::optional<int> from a string.
//   3. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: return std::nullopt; for the empty case. opt.value_or(default)
//       provides a fallback without dereferencing.

#include "cpplings.h"
#include <optional>
#include <string>
#include <vector>
#include <algorithm>

struct User {
    int id;
    std::string name;
    std::string email;
};

static std::vector<User> db = {
    {1, "Alice", "alice@example.com"},
    {2, "Bob", "bob@example.com"},
    {3, "Charlie", "charlie@example.com"},
};

// TODO: Return the User with matching id, or std::nullopt if not found.
std::optional<User> find_user(int id) {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Hint: use std::find_if on db, check against db.end()
    return std::nullopt;  // replace this
}

// TODO: Parse a string into an int. Return std::nullopt on failure.
std::optional<int> parse_int(const std::string& s) {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Hint: std::stoi with pos checking, or manual digit loop
    return std::nullopt;  // replace this
}

TEST("optional — has value") {
    auto user = find_user(1);
    ASSERT_TRUE(user.has_value());
    ASSERT_EQ(user->name, "Alice");
    ASSERT_EQ(user->email, "alice@example.com");
}

TEST("optional — no value") {
    auto user = find_user(999);
    ASSERT_FALSE(user.has_value());
    ASSERT_TRUE(!user);
}

TEST("optional — value_or default") {
    auto existing = find_user(2);
    auto missing = find_user(999);

    ASSERT_EQ(existing.value_or(User{-1, "unknown", ""}).name, "Bob");
    ASSERT_EQ(missing.value_or(User{-1, "unknown", ""}).name, "unknown");
}

TEST("optional — parsing") {
    auto good = parse_int("42");
    auto bad = parse_int("abc");
    auto empty = parse_int("");

    ASSERT_TRUE(good.has_value());
    ASSERT_EQ(good.value(), 42);
    ASSERT_FALSE(bad.has_value());
    ASSERT_FALSE(empty.has_value());
}

TEST("optional — manual chaining (C++17)") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // TODO: Get the email of user 3 using optional chaining.
    //   C++23: find_user(3).and_then(...)
    //   C++17: manual — check has_value, then extract
    // auto result = ...;
    // ASSERT_TRUE(result.has_value());
    // ASSERT_EQ(result.value(), "charlie@example.com");
}

TEST("optional — emplace and reset") {
    std::optional<std::string> opt;
    ASSERT_FALSE(opt.has_value());

    opt = "hello";
    ASSERT_TRUE(opt.has_value());
    ASSERT_EQ(*opt, "hello");

    opt.reset();
    ASSERT_FALSE(opt.has_value());
}

CPPLINGS_MAIN
