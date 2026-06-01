// Exercise: 自定义哈希
// std::unordered_map 默认只能哈希标准类型。
// 要让自定义类型作为键，需要特化 std::hash 或提供自定义哈希器。
//
// 任务:
//   1. 特化 std::hash<Point> — 组合 x 和 y 的哈希值
//   2. 实现 CaseInsensitiveHash — 自定义哈希器作为模板参数
//   3. 实现 CaseInsensitiveEqual — 自定义相等谓词
//   4. 使用 unordered_map 配合自定义类型
//
// 提示: 哈希组合模式: h1 ^ (h2 << 1) 将两个哈希混合
//       std::hash 特化必须放在 std 命名空间中
//       unordered_map 的第 3 个模板参数是哈希器
//       unordered_map 的第 4 个模板参数是相等谓词

#include "cpplings.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <cctype>

struct Point {
    int x;
    int y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// TODO 1: 在 std 命名空间中特化 std::hash<Point>
//   namespace std {
//   template <>
//   struct hash<Point> {
//       size_t operator()(const Point& p) const {
//           size_t h1 = std::hash<int>()(p.x);
//           size_t h2 = std::hash<int>()(p.y);
//           return h1 ^ (h2 << 1);  // 哈希组合模式
//       }
//   };
//   }
//
//   注意: h1 ^ (h2 << 1) 是常见的哈希组合方式，
//   比简单的异或更不容易产生碰撞。

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 2: 实现 CaseInsensitiveHash
//   struct CaseInsensitiveHash {
//       size_t operator()(const std::string& s) const {
//           size_t hash = 0;
//           for (char c : s) {
//               hash = hash * 31 + std::tolower(static_cast<unsigned char>(c));
//           }
//           return hash;
//       }
//   };
//
//   使用简单的多项式哈希，所有字符先转为小写。

int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 3: 实现 CaseInsensitiveEqual
//   struct CaseInsensitiveEqual {
//       bool operator()(const std::string& a, const std::string& b) const {
//           if (a.size() != b.size()) return false;
//           for (size_t i = 0; i < a.size(); ++i) {
//               if (std::tolower(static_cast<unsigned char>(a[i])) !=
//                   std::tolower(static_cast<unsigned char>(b[i])))
//                   return false;
//           }
//           return true;
//       }
//   };

int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// --- Tests ---

TEST("Point 哈希可用 unordered_map") {
    // 如果 std::hash<Point> 未特化，这行无法编译
    std::unordered_map<Point, std::string> locations;

    locations[{0, 0}] = "origin";
    locations[{1, 2}] = "point_a";
    locations[{3, 4}] = "point_b";

    ASSERT_EQ(locations.size(), 3u);
    ASSERT_EQ(locations[{0, 0}], std::string("origin"));
    ASSERT_EQ(locations[{1, 2}], std::string("point_a"));
}

TEST("Point find 和 insert") {
    std::unordered_map<Point, int> distances;
    Point p1{10, 20};
    Point p2{30, 40};
    Point p3{10, 20};  // 与 p1 相等

    distances[p1] = 100;
    distances[p2] = 200;

    // p3 应该找到 p1 的位置
    auto it = distances.find(p3);
    ASSERT_TRUE(it != distances.end());
    ASSERT_EQ(it->second, 100);

    // 查找不存在的点
    Point p4{99, 99};
    ASSERT_TRUE(distances.find(p4) == distances.end());
}

TEST("哈希组合 — 不同点产生不同哈希（大概率）") {
    std::hash<Point> hasher;
    Point p1{1, 2};
    Point p2{2, 1};
    Point p3{3, 4};

    // 不同的点应该（大概率）有不同的哈希值
    // 虽然不能保证 100%，但这些输入差异足够大
    ASSERT_TRUE(hasher(p1) != hasher(p3));
    // p1 和 p2 虽然数值相同但位置不同，哈希应该不同
    ASSERT_TRUE(hasher(p1) != hasher(p2));
}

TEST("大小写不敏感的 unordered_map") {
    std::unordered_map<std::string, int,
                       CaseInsensitiveHash,
                       CaseInsensitiveEqual> scores;

    scores["Alice"] = 95;
    scores["Bob"] = 88;

    // 用不同大小写查找
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

    // 相等的字符串必须有相同的哈希值
    ASSERT_TRUE(eq(a, b));
    ASSERT_TRUE(eq(a, c));
    ASSERT_EQ(hasher(a), hasher(b));
    ASSERT_EQ(hasher(a), hasher(c));
}

CPPLINGS_MAIN
