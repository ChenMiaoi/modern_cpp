// cpplings: flatmap1
// 主题: C++23 — std::flat_map / std::flat_set
//
// TODO: 实现 FlatMap<K,V> 和 FlatSet<T>
// 使用有序 vector + 二分查找实现
//
// 提示: flat_map 内部用两个平行的 sorted vector
//       插入时找到正确位置并保持有序
//       查找时使用二分搜索

#include "cpplings.h"
#include <vector>
#include <string>
#include <algorithm>
#include <utility>

// TODO: 实现 FlatMap<K, V>
template <typename K, typename V>
class FlatMap {
    std::vector<K> keys_;
    std::vector<V> values_;

public:
    // TODO: insert — 插入键值对，如果键已存在则不插入
    // 返回 true 表示插入成功，false 表示键已存在
    bool insert(const K& key, const V& value) {
        int _todo_ = "FILL IN THE TODO";
        return false;
    }

    // TODO: find — 查找键，返回指向值的指针，未找到返回 nullptr
    const V* find(const K& key) const {
        int _todo_ = "FILL IN THE TODO";
        return nullptr;
    }

    // TODO: at — 查找键，找到返回值引用，未找到抛出 std::out_of_range
    const V& at(const K& key) const {
        int _todo_ = "FILL IN THE TODO";
        throw std::out_of_range("not found");
    }

    // TODO: size — 返回元素数量
    std::size_t size() const {
        int _todo_ = "FILL IN THE TODO";
        return 0;
    }

    // TODO: erase — 删除键，返回是否成功
    bool erase(const K& key) {
        int _todo_ = "FILL IN THE TODO";
        return false;
    }

    // 已提供: 获取键列表（已排序）
    const std::vector<K>& keys() const { return keys_; }
    const std::vector<V>& values() const { return values_; }
};

// TODO: 实现 FlatSet<T>
template <typename T>
class FlatSet {
    std::vector<T> data_;

public:
    // TODO: insert — 插入元素，保持有序，重复则忽略
    bool insert(const T& value) {
        int _todo_ = "FILL IN THE TODO";
        return false;
    }

    // TODO: contains — 是否包含元素
    bool contains(const T& value) const {
        int _todo_ = "FILL IN THE TODO";
        return false;
    }

    // TODO: size — 返回元素数量
    std::size_t size() const {
        int _todo_ = "FILL IN THE TODO";
        return 0;
    }

    // 已提供: 获取内部数据
    const std::vector<T>& data() const { return data_; }
};

TEST("FlatMap insert 和 find") {
    FlatMap<std::string, int> fm;
    fm.insert("one", 1);
    fm.insert("two", 2);
    fm.insert("three", 3);

    ASSERT_EQ(fm.size(), 3u);
    ASSERT_TRUE(fm.find("one") != nullptr);
    ASSERT_EQ(*fm.find("one"), 1);
    ASSERT_EQ(*fm.find("two"), 2);
    ASSERT_TRUE(fm.find("four") == nullptr);
}

TEST("FlatMap 键保持有序") {
    FlatMap<int, std::string> fm;
    fm.insert(3, "three");
    fm.insert(1, "one");
    fm.insert(2, "two");

    ASSERT_EQ(fm.keys()[0], 1);
    ASSERT_EQ(fm.keys()[1], 2);
    ASSERT_EQ(fm.keys()[2], 3);
}

TEST("FlatMap at 访问和异常") {
    FlatMap<std::string, int> fm;
    fm.insert("x", 10);
    ASSERT_EQ(fm.at("x"), 10);
    ASSERT_THROWS(fm.at("y"), std::out_of_range);
}

TEST("FlatMap erase") {
    FlatMap<int, int> fm;
    fm.insert(1, 10);
    fm.insert(2, 20);
    ASSERT_TRUE(fm.erase(1));
    ASSERT_FALSE(fm.erase(1));  // 已删除
    ASSERT_EQ(fm.size(), 1u);
    ASSERT_TRUE(fm.find(2) != nullptr);
}

TEST("FlatMap 重复插入忽略") {
    FlatMap<std::string, int> fm;
    ASSERT_TRUE(fm.insert("a", 1));
    ASSERT_FALSE(fm.insert("a", 2));  // 键已存在
    ASSERT_EQ(*fm.find("a"), 1);      // 值不变
}

TEST("FlatSet insert 和 contains") {
    FlatSet<int> fs;
    ASSERT_TRUE(fs.insert(3));
    ASSERT_TRUE(fs.insert(1));
    ASSERT_TRUE(fs.insert(2));
    ASSERT_FALSE(fs.insert(2));  // 重复

    ASSERT_EQ(fs.size(), 3u);
    ASSERT_TRUE(fs.contains(1));
    ASSERT_TRUE(fs.contains(2));
    ASSERT_TRUE(fs.contains(3));
    ASSERT_FALSE(fs.contains(4));
}

TEST("FlatSet 保持有序") {
    FlatSet<int> fs;
    fs.insert(5);
    fs.insert(1);
    fs.insert(3);

    ASSERT_EQ(fs.data()[0], 1);
    ASSERT_EQ(fs.data()[1], 3);
    ASSERT_EQ(fs.data()[2], 5);
}

CPPLINGS_MAIN
