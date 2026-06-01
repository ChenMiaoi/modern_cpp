// Solution: 无序容器 (unordered_map / unordered_set)
// C++11 引入了基于哈希表的无序关联容器，
// 提供平均 O(1) 的查找、插入和删除。

#include "cpplings.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

TEST("unordered_map 基本操作") {
    std::unordered_map<std::string, int> ages;
    ages.insert({"Alice", 30});
    ages.insert({"Bob", 25});
    ages.insert({"Charlie", 35});

    ASSERT_EQ(ages.size(), 3u);
    ASSERT_EQ(ages["Alice"], 30);
    ASSERT_EQ(ages["Bob"], 25);
    ASSERT_EQ(ages["Charlie"], 35);
}

TEST("unordered_map find 和 count") {
    std::unordered_map<std::string, int> scores = {
        {"math", 95}, {"english", 88}, {"physics", 92}
    };

    bool found = scores.count("math") > 0;
    bool not_found = scores.count("chemistry") > 0;

    ASSERT_TRUE(found);
    ASSERT_FALSE(not_found);
}

TEST("unordered_map erase") {
    std::unordered_map<std::string, int> m = {
        {"a", 1}, {"b", 2}, {"c", 3}
    };

    m.erase("b");

    ASSERT_EQ(m.size(), 2u);
    ASSERT_EQ(m.count("b"), 0u);
}

TEST("unordered_set 去重") {
    std::unordered_set<int> s;
    int data[] = {1, 2, 3, 2, 1, 4, 3};
    for (int x : data) {
        s.insert(x);
    }

    ASSERT_EQ(s.size(), 4u);
    ASSERT_EQ(s.count(1), 1u);
    ASSERT_EQ(s.count(2), 1u);
    ASSERT_EQ(s.count(5), 0u);
}

TEST("bucket_count 信息") {
    std::unordered_map<int, int> m;
    for (int i = 0; i < 100; ++i) {
        m[i] = i * 2;
    }

    auto bc = m.bucket_count();
    auto sz = m.size();

    ASSERT_EQ(sz, 100u);
    ASSERT_TRUE(bc >= 100u);
}

CPPLINGS_MAIN
