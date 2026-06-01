// cpplings: smartptr1
// 主题: C++11 类与资源管理 — unique_ptr
//
// TODO: 使用 std::unique_ptr 管理动态对象
// 让所有断言通过
//
// 提示: std::make_unique<T>(args...) 创建对象返回 unique_ptr。
//       用 std::move() 转移所有权，转移后原指针变为空。
//       unique_ptr 不能拷贝，只能移动。

#include "cpplings.h"
#include <memory>
#include <string>

struct Animal {
    std::string name;
    int legs;
    Animal(std::string n, int l) : name(std::move(n)), legs(l) {}
    virtual ~Animal() = default;
    virtual std::string speak() const = 0;
};

struct Dog : Animal {
    Dog(std::string n) : Animal(std::move(n), 4) {}
    std::string speak() const override { return "Woof!"; }
};

struct Cat : Animal {
    Cat(std::string n) : Animal(std::move(n), 4) {}
    std::string speak() const override { return "Meow!"; }
};

struct Spider : Animal {
    Spider(std::string n) : Animal(std::move(n), 8) {}
    std::string speak() const override { return "..."; }
};

// TODO: 实现工厂函数
// type 为 "dog"/"cat"/"spider" 时返回对应类型的 unique_ptr
// 其他 type 返回 nullptr
std::unique_ptr<Animal> make_animal(const std::string& type, const std::string& name) {
    int _todo_ = "FILL IN THE TODO";
    return nullptr;
}

TEST("make_unique 创建对象") {
    // TODO: 使用 std::make_unique 创建一个名字为 "Rex" 的 Dog，赋值给 p
    int _todo_ = "FILL IN THE TODO";

    ASSERT_EQ(p->name, "Rex");
    ASSERT_EQ(p->legs, 4);
    ASSERT_EQ(p->speak(), "Woof!");
}

TEST("工厂函数返回 unique_ptr") {
    // TODO: 用 make_animal 创建 dog("Buddy") 赋值给 d，cat("Whiskers") 赋值给 c
    int _todo_ = "FILL IN THE TODO";

    ASSERT_TRUE(d != nullptr);
    ASSERT_TRUE(c != nullptr);
    ASSERT_EQ(d->speak(), "Woof!");
    ASSERT_EQ(c->speak(), "Meow!");
}

TEST("unique_ptr 所有权转移") {
    auto p1 = std::make_unique<Spider>("Charlotte");
    ASSERT_TRUE(p1 != nullptr);

    // TODO: 使用 std::move 将 p1 的所有权转移到 p2
    int _todo_ = "FILL IN THE TODO";

    ASSERT_TRUE(p1 == nullptr);
    ASSERT_TRUE(p2 != nullptr);
    ASSERT_EQ(p2->legs, 8);
}

TEST("make_unique 防止泄漏") {
    // TODO: 使用 std::make_unique（不是 new）创建 Dog("Fido")，赋值给 p
    int _todo_ = "FILL IN THE TODO";

    ASSERT_EQ(p->name, "Fido");
}

TEST("未知类型返回 nullptr") {
    // TODO: 用 make_animal("fish", "Nemo") 创建对象，赋值给 unknown
    int _todo_ = "FILL IN THE TODO";

    ASSERT_TRUE(unknown == nullptr);
}

CPPLINGS_MAIN
