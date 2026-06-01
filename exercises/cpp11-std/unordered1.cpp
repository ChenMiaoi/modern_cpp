// Exercise: 无序容器 (unordered_map / unordered_set)
// C++11 引入了基于哈希表的无序关联容器，
// 提供平均 O(1) 的查找、插入和删除。
//
// 任务:
//   1. 使用 unordered_map<string, int> 存储键值对
//   2. 使用 insert、find、erase 操作
//   3. 使用 unordered_set 存储不重复元素
//   4. 使用 count 检查元素是否存在
//   5. 查看 bucket_count 等哈希表信息
//
// 提示: #include <unordered_map>
//       #include <unordered_set>
//       map.insert({"key", value});
//       auto it = map.find("key");  // 返回迭代器
//       map.erase("key");
//       set.count(x) 返回 0 或 1

#include "cpplings.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

TEST("unordered_map 基本操作") {
    // TODO: 创建 unordered_map<string, int> ages
    //       插入 {"Alice", 30}, {"Bob", 25}, {"Charlie", 35}

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(ages.size(), 3u);
    ASSERT_EQ(ages["Alice"], 30);
    ASSERT_EQ(ages["Bob"], 25);
    ASSERT_EQ(ages["Charlie"], 35);
}

TEST("unordered_map find 和 count") {
    std::unordered_map<std::string, int> scores = {
        {"math", 95}, {"english", 88}, {"physics", 92}
    };

    // TODO: 用 found 判断 scores 中是否存在 "math" 这个键
    //       用 not_found 判断 scores 中是否存在 "chemistry" 这个键
    // 提示: count() 返回 0 或 1

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_TRUE(found);
    ASSERT_FALSE(not_found);
}

TEST("unordered_map erase") {
    std::unordered_map<std::string, int> m = {
        {"a", 1}, {"b", 2}, {"c", 3}
    };

    // TODO: 删除键 "b" 对应的元素

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(m.size(), 2u);
    ASSERT_EQ(m.count("b"), 0u);
}

TEST("unordered_set 去重") {
    // TODO: 创建 unordered_set<int> s
    //       插入 {1, 2, 3, 2, 1, 4, 3} — 重复元素应被忽略

    int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

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

    // TODO: 用变量 bc 获取 m 的 bucket 数量
    //       用变量 sz 获取 m 的元素数量
    // 提示: bucket_count() 和 size()

    int _todo5_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(sz, 100u);
    ASSERT_TRUE(bc >= 100u);  // bucket 数应 >= 元素数
}

CPPLINGS_MAIN
