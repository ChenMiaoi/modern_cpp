// cpplings: cachefriendly1 — 解答
// 主题: 缓存友好的数据结构 — AoS vs SoA, alignment, flat_map

#include "cpplings.h"
#include <type_traits>
#include <vector>
#include <array>
#include <cstddef>
#include <algorithm>
#include <numeric>

// === AoS vs SoA ===

struct ParticleAoS {
    float x, y, z;
    float vx, vy, vz;
    float mass;
};

struct ParticlesSoA {
    std::vector<float> x, y, z;
    std::vector<float> vx, vy, vz;
    std::vector<float> mass;

    void resize(std::size_t n) {
        x.resize(n);
        y.resize(n);
        z.resize(n);
        vx.resize(n);
        vy.resize(n);
        vz.resize(n);
        mass.resize(n);
    }

    std::size_t size() const {
        return x.size();
    }
};

// === Cache line alignment ===

constexpr std::size_t CACHE_LINE_SIZE = 64;

struct alignas(CACHE_LINE_SIZE) AlignedCounter {
    long long value;

    AlignedCounter() : value(0) {}
    void increment() { ++value; }
};

struct UnalignedCounter {
    long long value;

    UnalignedCounter() : value(0) {}
    void increment() { ++value; }
};

// === Sequential vs Random access pattern ===

int sequential_sum(const std::vector<int>& v) {
    int sum = 0;
    for (auto val : v) {
        sum += val;
    }
    return sum;
}

int strided_sum(const std::vector<int>& v, std::size_t step) {
    int sum = 0;
    for (std::size_t i = 0; i < v.size(); i += step) {
        sum += v[i];
    }
    return sum;
}

// === flat_map ===

template <typename K, typename V>
class flat_map {
public:
    using pair_type = std::pair<K, V>;
    using iterator = typename std::vector<pair_type>::iterator;
    using const_iterator = typename std::vector<pair_type>::const_iterator;

    void insert(const K& key, const V& value) {
        auto it = std::lower_bound(data_.begin(), data_.end(), key,
            [](const pair_type& p, const K& k) { return p.first < k; });
        if (it != data_.end() && it->first == key) {
            it->second = value;  // update
        } else {
            data_.insert(it, pair_type(key, value));
        }
    }

    const_iterator find(const K& key) const {
        auto it = std::lower_bound(data_.begin(), data_.end(), key,
            [](const pair_type& p, const K& k) { return p.first < k; });
        if (it != data_.end() && it->first == key) {
            return it;
        }
        return data_.end();
    }

    std::size_t size() const {
        return data_.size();
    }

    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }

private:
    std::vector<pair_type> data_;
};

// === Tests ===

TEST("SoA 布局 — 同类数据连续存储") {
    ParticlesSoA particles;
    particles.resize(3);

    ASSERT_EQ(particles.size(), 3u);
}

TEST("AlignedCounter 对齐到缓存行") {
    static_assert(sizeof(AlignedCounter) >= CACHE_LINE_SIZE,
                  "AlignedCounter 应至少占一个缓存行");
    static_assert(alignof(AlignedCounter) == CACHE_LINE_SIZE,
                  "AlignedCounter 应对齐到缓存行边界");
    ASSERT_TRUE(true);
}

TEST("UnalignedCounter 不需要缓存行对齐") {
    static_assert(sizeof(UnalignedCounter) < CACHE_LINE_SIZE,
                  "UnalignedCounter 应小于缓存行");
    static_assert(alignof(UnalignedCounter) <= 8,
                  "UnalignedCounter 默认对齐");
    ASSERT_TRUE(true);
}

TEST("顺序求和正确") {
    std::vector<int> v = {1, 2, 3, 4, 5};
    ASSERT_EQ(sequential_sum(v), 15);
}

TEST("步长求和正确") {
    std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ASSERT_EQ(strided_sum(v, 2), 25);
    ASSERT_EQ(strided_sum(v, 3), 22);
}

TEST("flat_map 插入和查找") {
    flat_map<int, std::string> fm;
    fm.insert(3, "three");
    fm.insert(1, "one");
    fm.insert(2, "two");

    ASSERT_EQ(fm.size(), 3u);
    auto it = fm.find(2);
    ASSERT_TRUE(it != fm.end());
    ASSERT_EQ(it->second, "two");
}

TEST("flat_map 更新已有键") {
    flat_map<int, int> fm;
    fm.insert(1, 10);
    fm.insert(1, 20);
    ASSERT_EQ(fm.size(), 1u);
    ASSERT_EQ(fm.find(1)->second, 20);
}

TEST("flat_map 查找不存在的键") {
    flat_map<int, int> fm;
    fm.insert(1, 10);
    auto it = fm.find(99);
    ASSERT_TRUE(it == fm.end());
}

TEST("flat_map 有序迭代") {
    flat_map<int, int> fm;
    fm.insert(3, 30);
    fm.insert(1, 10);
    fm.insert(2, 20);

    std::vector<int> keys;
    for (const auto& [k, v] : fm) {
        keys.push_back(k);
    }
    ASSERT_EQ(keys.size(), 3u);
    ASSERT_EQ(keys[0], 1);
    ASSERT_EQ(keys[1], 2);
    ASSERT_EQ(keys[2], 3);
}

TEST("SoA vs AoS 布局验证 — SoA stride 更小") {
    static_assert(sizeof(ParticleAoS) == 7 * sizeof(float),
                  "AoS 每个粒子 7 个 float");
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
