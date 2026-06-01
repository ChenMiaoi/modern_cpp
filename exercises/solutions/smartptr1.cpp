// cpplings: smartptr1 — 解答
// 主题: C++11 类与资源管理 — unique_ptr

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

// 工厂函数：根据 type 返回对应的 unique_ptr<Animal>
std::unique_ptr<Animal> make_animal(const std::string& type, const std::string& name) {
    if (type == "dog")    return std::make_unique<Dog>(name);
    if (type == "cat")    return std::make_unique<Cat>(name);
    if (type == "spider") return std::make_unique<Spider>(name);
    return nullptr;
}

TEST("make_unique 创建对象") {
    auto p = std::make_unique<Dog>("Rex");
    ASSERT_EQ(p->name, "Rex");
    ASSERT_EQ(p->legs, 4);
    ASSERT_EQ(p->speak(), "Woof!");
}

TEST("工厂函数返回 unique_ptr") {
    auto d = make_animal("dog", "Buddy");
    auto c = make_animal("cat", "Whiskers");

    ASSERT_TRUE(d != nullptr);
    ASSERT_TRUE(c != nullptr);
    ASSERT_EQ(d->speak(), "Woof!");
    ASSERT_EQ(c->speak(), "Meow!");
}

TEST("unique_ptr 所有权转移") {
    auto p1 = std::make_unique<Spider>("Charlotte");
    ASSERT_TRUE(p1 != nullptr);

    auto p2 = std::move(p1);

    ASSERT_TRUE(p1 == nullptr);
    ASSERT_TRUE(p2 != nullptr);
    ASSERT_EQ(p2->legs, 8);
}

TEST("make_unique 防止泄漏") {
    auto p = std::make_unique<Dog>("Fido");
    ASSERT_EQ(p->name, "Fido");
}

TEST("未知类型返回 nullptr") {
    auto unknown = make_animal("fish", "Nemo");
    ASSERT_TRUE(unknown == nullptr);
}

CPPLINGS_MAIN
