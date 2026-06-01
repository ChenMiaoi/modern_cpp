// cpplings: smartptr2
// 主题: C++11 类与资源管理 — shared_ptr 与 weak_ptr
//
// TODO: 使用 shared_ptr 和 weak_ptr 正确管理共享资源
// 让所有断言通过
//
// 提示: shared_ptr 使用引用计数，拷贝时计数 +1，销毁时 -1。
//       weak_ptr 观察对象但不增加引用计数，用 lock() 获取 shared_ptr。
//       weak_ptr 用于打破循环引用：父→子用 shared_ptr，子→父用 weak_ptr。

#include "cpplings.h"
#include <memory>
#include <string>
#include <vector>

TEST("shared_ptr 引用计数") {
    auto p = std::make_shared<int>(42);
    ASSERT_EQ(p.use_count(), 1);

    {
        // TODO: 拷贝 p 到 q，使引用计数变为 2
        int _todo_ = "FILL IN THE TODO";

        ASSERT_EQ(p.use_count(), 2);
        ASSERT_EQ(*q, 42);
    }

    // q 离开作用域后引用计数回到 1
    ASSERT_EQ(p.use_count(), 1);
}

TEST("shared_ptr 共享所有权") {
    auto shared = std::make_shared<std::string>("hello");

    // TODO: 创建 vector<shared_ptr<string>> 类型的 holders
    //       将 shared push_back 两次，使引用计数达到 3
    int _todo_ = "FILL IN THE TODO";

    ASSERT_EQ(shared.use_count(), 3);
    ASSERT_EQ(*holders[0], "hello");

    holders.clear();
    ASSERT_EQ(shared.use_count(), 1);
}

TEST("weak_ptr 不增加引用计数") {
    std::weak_ptr<int> wp;
    {
        auto sp = std::make_shared<int>(100);

        // TODO: 用 sp 初始化 wp（weak_ptr 观察但不拥有资源）
        int _todo_ = "FILL IN THE TODO";

        ASSERT_EQ(sp.use_count(), 1);  // weak_ptr 不影响引用计数
        ASSERT_FALSE(wp.expired());
    }

    // sp 销毁后对象释放，wp 过期
    ASSERT_TRUE(wp.expired());
}

TEST("weak_ptr.lock() 安全获取共享指针") {
    std::weak_ptr<int> wp;

    // TODO: 对空的 wp 调用 lock()，结果赋值给 locked
    int _todo_ = "FILL IN THE TODO";
    ASSERT_TRUE(locked == nullptr);

    auto sp = std::make_shared<int>(99);
    wp = sp;

    // TODO: 对有效的 wp 调用 lock()，结果赋值给 locked2
    int _todo_ = "FILL IN THE TODO";
    ASSERT_TRUE(locked2 != nullptr);
    ASSERT_EQ(*locked2, 99);
}

TEST("打破循环引用") {
    auto parent = std::make_shared<int>(1);

    // TODO: 创建 weak_ptr<int> wp 观察 parent（不增加引用计数）
    int _todo_ = "FILL IN THE TODO";

    ASSERT_EQ(parent.use_count(), 1);
    ASSERT_FALSE(wp.expired());

    auto locked = wp.lock();
    ASSERT_EQ(*locked, 1);
}

CPPLINGS_MAIN
