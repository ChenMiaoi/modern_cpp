// 去虚拟化 (Devirtualization) 演示
// 编译器将虚函数调用转为直接调用
//
// 编译查看:
//   g++ -O2 -S devirt.cpp -o devirt.s
//   # 搜索 call 指令: 去虚拟化后是直接 call，不是间接 call (call *%rax)

struct Base {
    virtual int value() const { return 0; }
    virtual ~Base() = default;
};

struct Derived : Base {
    int value() const override { return 42; }
};

// final 类允许编译器去虚拟化
struct FinalDerived final : Base {
    int value() const override { return 99; }
};

// 编译器可以通过类型推导去虚拟化
int test_devirt() {
    Derived d;
    Base& b = d;
    // 编译器知道 b 的实际类型是 Derived
    // -O2 可能去虚拟化为直接调用 Derived::value()
    return b.value();
}

// final 类保证去虚拟化
int test_final() {
    FinalDerived fd;
    Base& b = fd;
    // FinalDerived 是 final，不可能有派生类
    // 编译器一定去虚拟化
    return b.value();
}

// 不可去虚拟化: 运行时才知道类型
int test_no_devirt(Base* p) {
    // p 可能是任何派生类，无法去虚拟化
    return p->value();
}

int main() {
    return test_devirt() + test_final();
}
