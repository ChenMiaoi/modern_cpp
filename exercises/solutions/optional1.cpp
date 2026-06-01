// Solution: std::optional

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

std::optional<User> find_user(int id) {
    auto it = std::find_if(db.begin(), db.end(),
        [id](const User& u) { return u.id == id; });
    if (it != db.end()) {
        return *it;
    }
    return std::nullopt;
}

std::optional<int> parse_int(const std::string& s) {
    if (s.empty()) return std::nullopt;
    try {
        std::size_t pos = 0;
        int val = std::stoi(s, &pos);
        if (pos != s.size()) return std::nullopt;
        return val;
    } catch (...) {
        return std::nullopt;
    }
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
    auto user = find_user(3);
    std::optional<std::string> result;
    if (user.has_value()) {
        result = user->email;
    }
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), "charlie@example.com");
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
