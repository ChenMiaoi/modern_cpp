---
title: "C++ 术语黑话全书"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++ 术语黑话全书

> C++ 社区有大量专业术语——它们在标准文档、编译器实现、库设计和工程师交流中反复出现，但很少有人系统性地解释它们。本专题试图把这些"黑话"全部挖掘出来，逐一讲解。

这些术语不是凭空发明的——每一个都对应着一个具体的语言机制、一个可观察的行为差异、或一个影响代码正确性的微妙规则。理解它们，就是理解 C++ 的运作方式。

---

## 术语分类导航

### [值类别与表达式](/topics/cpp-jargon/value-categories)
lvalue、prvalue、xvalue、glvalue、rvalue、materialization、temporary materialization

### [对象模型与内存](/topics/cpp-jargon/object-model)
lifetime、storage duration、alignment、object representation、pointer invalidation、dangling reference、strict aliasing、placement new、std::launder

### [重载决议与名字查找](/topics/cpp-jargon/overload-resolution)
overload resolution、ADL (Argument-Dependent Lookup)、name hiding、two-phase lookup、dependent name、name mangling

### [模板机制](/topics/cpp-jargon/template-mechanics)
SFINAE、CRTP、CTAD、deduction guide、explicit specialization、partial specialization、variadic template、parameter pack、fold expression、expression template、template template parameter、requires clause、concept、subsumption、if constexpr

### [类型系统](/topics/cpp-jargon/type-system)
type erasure、type punning、type traits、tag dispatching、polymorphism (static/dynamic)、covariance、contravariance、invariant、UB/type mismatch

### [构造、析构与特殊成员](/topics/cpp-jargon/special-members)
Rule of Zero/Three/Five、copy elision、NRVO、RVO、guaranteed copy elision、trivially copyable、trivially relocatable、aggregate initialization、brace elision

### [异常安全](/topics/cpp-jargon/exception-safety)
exception safety guarantee (basic/strong/nothrow)、RAII、scope guard、noexcept、stack unwinding、exception specification

### [并发与内存模型](/topics/cpp-jargon/concurrency-terms)
data race、race condition、happens-before、sequenced before、memory order (relaxed/acquire/release/seq_cst)、atomic、lock-free、ABA problem、false sharing、cache line、memory barrier

### [编译与链接](/topics/cpp-jargon/compilation)
translation unit、ODR (One Definition Rule)、linkage (internal/external/no)、static initialization order fiasco、ABI、mangling、PCH、LTO、include guard、forward declaration、PImpl

### [优化与性能惯用语](/topics/cpp-jargon/optimization-terms)
copy elision、RVO、NRVO、small buffer optimization (SBO)、small string optimization (SSO)、copy-on-write (COW)、expression template、lazy evaluation、branch prediction、devirtualization、cache-friendly、prefetch、inline、LTO

### [标准库惯用语](/topics/cpp-jargon/stdlib-idioms)
RAII handle、sentinel、range、view、pipe operator、CPO (Customization Point Object)、niebloid、tag_invoke、allocator model、PMR、smart pointer (unique/shared/weak)

### [UB 与安全](/topics/cpp-jargon/ub-safety)
undefined behavior、implementation-defined、unspecified behavior、nasal demons、signed overflow、null dereference、use-after-free、buffer overflow、strict aliasing violation、std::launder

---

## 按使用频率排序的 Top 30 术语

| 排名 | 术语 | 一句话解释 |
|------|------|-----------|
| 1 | **RAII** | 资源在构造时获取、析构时释放——C++ 的基石 |
| 2 | **移动语义** | `std::move` 不移动任何东西，只是把左值转为右值引用 |
| 3 | **SFINAE** | 模板替换失败不是错误，而是回退到其他重载 |
| 4 | **值类别** | lvalue 有地址可取、prvalue 是纯值、xvalue 是"将亡值" |
| 5 | **ADL** | 编译器在参数所在的命名空间中查找函数 |
| 6 | **ODR** | 每个实体在整个程序中只能有一个定义 |
| 7 | **Copy Elision** | C++17 起 prvalue 不创建临时对象，直接构造到目标 |
| 8 | **SBO/SSO** | 小对象/字符串内联存储在栈上，避免堆分配 |
| 9 | **noexcept** | 承诺不抛异常——编译器据此优化，移动操作应标记 |
| 10 | **CRTP** | 基类模板参数是派生类——编译期多态 |
| 11 | **类型擦除** | `std::function` 如何存储任意 callable 的核心技术 |
| 12 | **完美转发** | `std::forward<T>` 保持参数的值类别不变 |
| 13 | **异常安全** | basic/strong/notthrow 三级保证 |
| 14 | **CTAD** | C++17 类模板参数推导，不再需要 `make_xxx` |
| 15 | **Concept** | C++20 对模板参数的命名约束，取代 SFINAE 黑魔法 |
| 16 | **虚表** | `virtual` 函数通过函数指针表实现运行时分发 |
| 17 | **迭代器失效** | 容器操作后哪些迭代器仍然有效 |
| 18 | **Rule of Five** | 如果自定义了析构/拷贝/移动之一，通常需要定义全部五个 |
| 19 | **happens-before** | C++ 内存模型中判断操作顺序的关系 |
| 20 | **PImpl** | 指向实现的指针——隐藏实现、减少编译依赖 |
| 21 | **EBO** | 空基类优化——空类型成员不占空间 |
| 22 | **表达式模板** | 延迟计算的模板技术，避免中间临时对象 |
| 23 | **NRVO** | 命名返回值优化——编译器直接在调用者栈帧构造返回对象 |
| 24 | **Tag Dispatching** | 通过空类型标签选择最优实现路径 |
| 25 | **strict aliasing** | 不同类型的指针不能指向同一内存（除非允许的例外） |
| 26 | **UB** | 编译器可以对未定义行为做任何事——包括"正常工作" |
| 27 | **constexpr** | 编译期可求值的函数/变量 |
| 28 | **类型特征** | `std::is_same`、`std::enable_if` 等编译期类型查询 |
| 29 | **依赖名** | 模板中依赖模板参数的名字——需要 `typename` 消歧义 |
| 30 | **两阶段查找** | 模板定义时查找非依赖名，实例化时查找依赖名 |
