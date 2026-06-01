// cpplings: flatmap1 — 解答
// 主题: C++23 — std::flat_map / std::flat_set

#include "cpplings.h"
#include <vector>
#include <string>
#include <algorithm>
#include <utility>
#include <stdexcept>

template <typename K, typename V>
class FlatMap {
    std::vector<K> keys_;
    std::vector<V> values_;

public:
    bool insert(const K& key, const V& value) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key);
        auto idx = static_cast<std::size_t>(it - keys_.begin());
        if (it != keys_.end() && *it == key) return false;
        keys_.insert(it, key);
        values_.insert(values_.begin() + idx, value);
        return true;
    }

    const V* find(const K& key) const {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key);
        if (it != keys_.end() && *it == key) {
            auto idx = static_cast<std::size_t>(it - keys_.begin());
            return &values_[idx];
        }
        return nullptr;
    }

    const V& at(const K& key) const {
        const V* p = find(key);
        if (!p) throw std::out_of_range("key not found");
        return *p;
    }

    std::size_t size() const { return keys_.size(); }

    bool erase(const K& key) {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), key);
        if (it == keys_.end() || *it != key) return false;
        auto idx = static_cast<std::size_t>(it - keys_.begin());
        keys_.erase(it);
        values_.erase(values_.begin() + idx);
        return true;
    }

    const std::vector<K>& keys() const { return keys_; }
    const std::vector<V>& values() const { return values_; }
};

template <typename T>
class FlatSet {
    std::vector<T> data_;

public:
    bool insert(const T& value) {
        auto it = std::lower_bound(data_.begin(), data_.end(), value);
        if (it != data_.end() && *it == value) return false;
        data_.insert(it, value);
        return true;
    }

    bool contains(const T& value) const {
        return std::binary_search(data_.begin(), data_.end(), value);
    }

    std::size_t size() const { return data_.size(); }

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
    ASSERT_FALSE(fm.erase(1));
    ASSERT_EQ(fm.size(), 1u);
    ASSERT_TRUE(fm.find(2) != nullptr);
}

TEST("FlatMap 重复插入忽略") {
    FlatMap<std::string, int> fm;
    ASSERT_TRUE(fm.insert("a", 1));
    ASSERT_FALSE(fm.insert("a", 2));
    ASSERT_EQ(*fm.find("a"), 1);
}

TEST("FlatSet insert 和 contains") {
    FlatSet<int> fs;
    ASSERT_TRUE(fs.insert(3));
    ASSERT_TRUE(fs.insert(1));
    ASSERT_TRUE(fs.insert(2));
    ASSERT_FALSE(fs.insert(2));

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
