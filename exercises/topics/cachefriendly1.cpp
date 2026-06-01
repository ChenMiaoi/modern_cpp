// cpplings: cachefriendly1
// 主题: 缓存友好的数据结构 — AoS vs SoA, alignment, flat_map
//
// TODO: 实现缓存友好的数据布局和操作
//
// 提示: AoS (Array of Structures) — 成员交替存储，跨步访问时缓存不友好
//       SoA (Structure of Arrays) — 同类数据连续存储，遍历时缓存友好
//       alignas(N) — 控制对齐，影响缓存行利用
//       flat_map — 排序 vector，比 node-based map 更缓存友好
//       false sharing — 不同线程写同一缓存行不同位置导致竞争

#include "cpplings.h"
#include <type_traits>
#include <vector>
#include <array>
#include <cstddef>
#include <algorithm>
#include <numeric>

int _todo_ = "请删除此行，实现所有 TODO";  // 编译错误：类型不匹配

// === AoS vs SoA ===

// AoS 布局: 所有属性交错存储
struct ParticleAoS {
    float x, y, z;    // 位置
    float vx, vy, vz; // 速度
    float mass;        // 质量
};

// TODO: 实现 SoA 布局 — 每个属性独立存储为数组
struct ParticlesSoA {
    // TODO: 用 vector<float> 存储各属性，而非交错存储
    // 位置 x, y, z 各一个 vector
    // 速度 vx, vy, vz 各一个 vector
    // 质量 mass 一个 vector

    // TODO: resize 方法 — 调整所有数组大小
    void resize(std::size_t n) {
        // TODO: 调整每个 vector 的大小
    }

    // TODO: size 方法
    std::size_t size() const {
        // TODO: 返回粒子数
        return 0;
    }
};

// === Cache line alignment ===

// 缓存行通常是 64 字节
constexpr std::size_t CACHE_LINE_SIZE = 64;

// TODO: 实现一个计数器，对齐到缓存行以避免 false sharing
// 提示: 使用 alignas(CACHE_LINE_SIZE)
struct alignas(CACHE_LINE_SIZE) AlignedCounter {
    long long value;

    AlignedCounter() : value(0) {}

    void increment() { ++value; }
};

// TODO: 未对齐的计数器（用于对比）
struct UnalignedCounter {
    long long value;

    UnalignedCounter() : value(0) {}

    void increment() { ++value; }
};

// === Sequential vs Random access pattern ===

// TODO: 顺序遍历（缓存友好）
// 对 vector 每个元素执行操作
int sequential_sum(const std::vector<int>& v) {
    // TODO: 简单顺序遍历求和
    return 0;
}

// TODO: 步长遍历（缓存不友好模拟）
// 以 step 为步长访问元素
int strided_sum(const std::vector<int>& v, std::size_t step) {
    // TODO: 按 step 步长遍历求和
    return 0;
}

// === flat_map ===

// TODO: 实现简单的 flat_map — 排序的 vector<pair>
// 比 std::map (红黑树) 更缓存友好
template <typename K, typename V>
class flat_map {
    // TODO: 用 vector<pair<K,V>> 存储，按 key 排序
public:
    using pair_type = std::pair<K, V>;
    using iterator = typename std::vector<pair_type>::iterator;
    using const_iterator = typename std::vector<pair_type>::const_iterator;

    // TODO: insert — 插入键值对，保持排序
    void insert(const K& key, const V& value) {
        // TODO: 找到插入位置（二分查找），插入或更新
    }

    // TODO: find — 查找键，返回迭代器
    const_iterator find(const K& key) const {
        // TODO: 二分查找
        return data_.end();
    }

    // TODO: size
    std::size_t size() const {
        return 0;
    }

    // TODO: 迭代器
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
    // alignas 确保对象大小是缓存行的倍数
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
    // 步长 2: 访问索引 0,2,4,6,8 => 1+3+5+7+9 = 25
    ASSERT_EQ(strided_sum(v, 2), 25);
    // 步长 3: 访问索引 0,3,6,9 => 1+4+7+10 = 22
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
    fm.insert(1, 20);  // 更新
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
    // AoS: 遍历所有 x 需要跳过 7 个 float（28 字节步长）
    static_assert(sizeof(ParticleAoS) == 7 * sizeof(float),
                  "AoS 每个粒子 7 个 float");
    // SoA: x 数据连续存储，stride 是 sizeof(float) = 4 字节
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
