// cpplings: exception1
// 主题: 异常安全 — 三大保证、swap 惯用法、RAII、noexcept
//
// TODO: 实现异常安全的类和函数
//
// 提示: 基本保证 (basic) — 不泄漏资源，不变量可处于任意合法状态
//       强保证 (strong) — 操作要么成功，要么状态不变（事务语义）
//       不抛出保证 (nothrow) — 操作永不抛出异常
//       swap 惯用法天然提供强保证
//       RAII 确保异常路径下资源自动释放

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

int _todo_ = "请删除此行，实现所有 TODO";  // 编译错误：类型不匹配

// === RAII 包装器 ===
// 一个简单的资源管理类，跟踪构造和析构
struct ResourceTracker {
    static int alive;   // 当前存活的对象数
    static int created; // 累计创建的对象数

    int id;
    std::string data;

    ResourceTracker(const std::string& d) : id(++created), data(d) {
        ++alive;
    }

    // TODO: 实现拷贝构造函数，创建新 ResourceTracker
    // 提示: 分配新 id，复制 data，递增 alive
    ResourceTracker(const ResourceTracker& other) : id(/* TODO */), data(other.data) {
        // TODO: 递增 alive
    }

    // TODO: 实现移动构造函数，转移 data 所有权
    // 提示: 使用 std::move 转移 data，新对象分配新 id
    ResourceTracker(ResourceTracker&& other) noexcept : id(/* TODO */), data(std::move(other.data)) {
        // TODO: 递增 alive，将 other 标记为已移动（设 data 为空）
    }

    // TODO: 实现析构函数
    // 提示: 递减 alive
    ~ResourceTracker() {
        // TODO: 递减 alive
    }
};

// 静态成员初始化
int ResourceTracker::alive = 0;
int ResourceTracker::created = 0;

// === Swap 强保证 ===

// TODO: StrongGuarantee 类 — 使用 pimpl + swap 实现强异常保证
class StrongGuarantee {
public:
    StrongGuarantee(int val) : value_(val) {}

    // TODO: 实现 update 方法，使用 copy-and-swap 提供强保证
    // 创建临时副本，修改副本，然后 noexcept swap
    // 如果修改过程中抛出异常，原对象不变
    void update(int new_val) {
        // TODO: 创建 this 的副本
        // 修改副本的 value_
        // 使用 noexcept swap 交换
    }

    int value() const { return value_; }

    // TODO: 实现 noexcept swap
    void swap(StrongGuarantee& other) noexcept {
        // TODO: 交换 value_
    }

private:
    int value_;
};

// TODO: 非成员 swap 重载
// namespace std {
//     template<> void swap<StrongGuarantee>(StrongGuarantee& a, StrongGuarantee& b) noexcept {
//         a.swap(b);
//     }
// }

// === Noexcept 检测 ===

// TODO: 标记这些函数的 noexcept 状态
struct NoexceptDemo {
    int value;

    // TODO: 移动构造函数 — 标记为 noexcept
    NoexceptDemo(NoexceptDemo&& other) noexcept : value(other.value) {
        // TODO: 将 other.value 置为 0
    }

    // TODO: 拷贝构造函数 — 不标记 noexcept（可能抛异常）
    NoexceptDemo(const NoexceptDemo& other) : value(other.value) {}

    // TODO: 移动赋值 — 标记为 noexcept
    NoexceptDemo& operator=(NoexceptDemo&& other) noexcept {
        // TODO: 转移 value
        return *this;
    }

    // TODO: 析构函数 — 隐式 noexcept
    ~NoexceptDemo() = default;
};

// === Stack Unwinding RAII ===

// TODO: 实现一个 RAII 类，记录每个生命周期阶段
struct LifecycleTracker {
    static std::vector<std::string> events;

    std::string name;

    LifecycleTracker(const std::string& n) : name(n) {
        events.push_back("construct:" + name);
    }

    // TODO: 析构函数中记录 "destruct:<name>"
    ~LifecycleTracker() {
        // TODO: 记录析构事件
    }
};

std::vector<std::string> LifecycleTracker::events;

TEST("RAII 跟踪存活对象数") {
    ResourceTracker::alive = 0;
    ResourceTracker::created = 0;
    {
        ResourceTracker a("A");
        ResourceTracker b("B");
        ASSERT_EQ(ResourceTracker::alive, 2);
    }
    ASSERT_EQ(ResourceTracker::alive, 0);
}

TEST("ResourceTracker 拷贝构造增加计数") {
    ResourceTracker::alive = 0;
    ResourceTracker::created = 0;
    ResourceTracker a("orig");
    ResourceTracker b(a);
    ASSERT_EQ(ResourceTracker::alive, 2);
    ASSERT_EQ(b.data, "orig");
}

TEST("ResourceTracker 移动构造转移数据") {
    ResourceTracker::alive = 0;
    ResourceTracker::created = 0;
    ResourceTracker a("movable");
    ResourceTracker b(std::move(a));
    ASSERT_EQ(b.data, "movable");
    ASSERT_EQ(a.data, "");  // 被移动后为空
}

TEST("StrongGuarantee swap 提供强保证") {
    StrongGuarantee obj(10);
    obj.update(20);
    ASSERT_EQ(obj.value(), 20);
}

TEST("StrongGuarantee swap 是 noexcept") {
    ASSERT_TRUE(noexcept(std::declval<StrongGuarantee&>().swap(
        std::declval<StrongGuarantee&>())));
}

TEST("NoexceptDemo 移动构造是 noexcept") {
    ASSERT_TRUE(std::is_nothrow_move_constructible_v<NoexceptDemo>);
}

TEST("NoexceptDemo 拷贝构造不是 noexcept") {
    // 拷贝构造未标记 noexcept
    ASSERT_FALSE(noexcept(NoexceptDemo(std::declval<const NoexceptDemo&>())));
}

TEST("NoexceptDemo 移动赋值是 noexcept") {
    ASSERT_TRUE(std::is_nothrow_move_assignable_v<NoexceptDemo>);
}

TEST("Stack unwinding 调用析构函数") {
    LifecycleTracker::events.clear();
    {
        LifecycleTracker outer("outer");
        {
            LifecycleTracker inner("inner");
        }
        // inner 在此处析构
    }
    // outer 在此处析构
    ASSERT_TRUE(LifecycleTracker::events.size() >= 2);
    ASSERT_EQ(LifecycleTracker::events[0], "construct:outer");
    ASSERT_EQ(LifecycleTracker::events[1], "construct:inner");
    ASSERT_EQ(LifecycleTracker::events[2], "destruct:inner");
    ASSERT_EQ(LifecycleTracker::events[3], "destruct:outer");
}

TEST("RAII 析构在异常路径下执行") {
    ResourceTracker::alive = 0;
    ResourceTracker::created = 0;
    try {
        ResourceTracker r("protected");
        ASSERT_EQ(ResourceTracker::alive, 1);
        throw std::runtime_error("boom");
    } catch (...) {
        // r 应已被析构
    }
    ASSERT_EQ(ResourceTracker::alive, 0);
}

CPPLINGS_MAIN
