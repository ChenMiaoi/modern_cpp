// cpplings: exception1 — 解答
// 主题: 异常安全 — 三大保证、swap 惯用法、RAII、noexcept

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

// === RAII 包装器 ===
struct ResourceTracker {
    static int alive;
    static int created;

    int id;
    std::string data;

    ResourceTracker(const std::string& d) : id(++created), data(d) {
        ++alive;
    }

    ResourceTracker(const ResourceTracker& other) : id(++created), data(other.data) {
        ++alive;
    }

    ResourceTracker(ResourceTracker&& other) noexcept
        : id(++created), data(std::move(other.data)) {
        ++alive;
        other.data.clear();
    }

    ~ResourceTracker() {
        --alive;
    }
};

int ResourceTracker::alive = 0;
int ResourceTracker::created = 0;

// === Swap 强保证 ===

class StrongGuarantee {
public:
    StrongGuarantee(int val) : value_(val) {}

    void update(int new_val) {
        StrongGuarantee tmp(*this);  // copy
        tmp.value_ = new_val;        // modify copy (if this throws, *this unchanged)
        swap(tmp);                    // noexcept swap
    }

    int value() const { return value_; }

    void swap(StrongGuarantee& other) noexcept {
        std::swap(value_, other.value_);
    }

private:
    int value_;
};

// === Noexcept 检测 ===

struct NoexceptDemo {
    int value;

    NoexceptDemo(NoexceptDemo&& other) noexcept : value(other.value) {
        other.value = 0;
    }

    NoexceptDemo(const NoexceptDemo& other) : value(other.value) {}

    NoexceptDemo& operator=(NoexceptDemo&& other) noexcept {
        value = other.value;
        other.value = 0;
        return *this;
    }

    ~NoexceptDemo() = default;
};

// === Stack Unwinding RAII ===

struct LifecycleTracker {
    static std::vector<std::string> events;

    std::string name;

    LifecycleTracker(const std::string& n) : name(n) {
        events.push_back("construct:" + name);
    }

    ~LifecycleTracker() {
        events.push_back("destruct:" + name);
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
    ASSERT_EQ(a.data, "");
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
    }
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
    }
    ASSERT_EQ(ResourceTracker::alive, 0);
}

CPPLINGS_MAIN
