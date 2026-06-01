// cpplings: smartptr2 — 解答
// 主题: C++11 类与资源管理 — shared_ptr 与 weak_ptr

#include "cpplings.h"
#include <memory>
#include <string>
#include <vector>

TEST("shared_ptr 引用计数") {
    auto p = std::make_shared<int>(42);
    ASSERT_EQ(p.use_count(), 1);

    {
        auto q = p;  // 拷贝，引用计数 +1
        ASSERT_EQ(p.use_count(), 2);
        ASSERT_EQ(*q, 42);
    }

    // q 离开作用域后引用计数回到 1
    ASSERT_EQ(p.use_count(), 1);
}

TEST("shared_ptr 共享所有权") {
    auto shared = std::make_shared<std::string>("hello");

    std::vector<std::shared_ptr<std::string>> holders;
    holders.push_back(shared);
    holders.push_back(shared);

    ASSERT_EQ(shared.use_count(), 3);
    ASSERT_EQ(*holders[0], "hello");

    holders.clear();
    ASSERT_EQ(shared.use_count(), 1);
}

TEST("weak_ptr 不增加引用计数") {
    std::weak_ptr<int> wp;
    {
        auto sp = std::make_shared<int>(100);
        wp = sp;  // weak_ptr 观察但不拥有

        ASSERT_EQ(sp.use_count(), 1);  // weak_ptr 不影响引用计数
        ASSERT_FALSE(wp.expired());
    }

    // sp 销毁后对象释放，wp 过期
    ASSERT_TRUE(wp.expired());
}

TEST("weak_ptr.lock() 安全获取共享指针") {
    std::weak_ptr<int> wp;

    auto locked = wp.lock();         // 空 weak_ptr → nullptr
    ASSERT_TRUE(locked == nullptr);

    auto sp = std::make_shared<int>(99);
    wp = sp;

    auto locked2 = wp.lock();        // 有效 weak_ptr → shared_ptr
    ASSERT_TRUE(locked2 != nullptr);
    ASSERT_EQ(*locked2, 99);
}

TEST("打破循环引用") {
    auto parent = std::make_shared<int>(1);
    std::weak_ptr<int> wp = parent;  // weak_ptr 不增加引用计数

    ASSERT_EQ(parent.use_count(), 1);
    ASSERT_FALSE(wp.expired());

    auto locked = wp.lock();
    ASSERT_EQ(*locked, 1);
}

CPPLINGS_MAIN
