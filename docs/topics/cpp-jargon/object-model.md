---
title: "对象模型与内存术语"
topic: unknown
feature: object-model
standard: N/A
status_checked_at: 2026-06-02
---
# 对象模型与内存术语

## Lifetime（生命周期）

对象的生命周期从构造完成开始，到析构开始结束。在生命周期外访问对象是 **UB**。

```cpp
{
  int* p;
  {
    int x = 42;
    p = &x;
  }  // x 的生命周期结束
  // *p 是 UB！虽然内存可能还在，但对象已不存在
}
```

## Storage Duration（存储期）

| 存储期 | 创建时机 | 销毁时机 | 示例 |
|--------|---------|---------|------|
| automatic | 进入作用域 | 离开作用域 | 局部变量 |
| static | 程序启动 | 程序退出 | 全局变量、`static` 局部变量 |
| dynamic | `new` | `delete` | 堆分配对象 |
| thread | 线程创建 | 线程退出 | `thread_local` |

## Dangling Reference（悬垂引用）

引用的对象已销毁，但引用仍然存在：

```cpp
const std::string& bad() {
  std::string s = "hello";
  return s;  // 悬垂引用！s 在函数返回后销毁
}

std::string_view bad_view() {
  std::string s = "hello";
  return s;  // string_view 悬垂！s 销毁后 view 指向无效内存
}
```

## Strict Aliasing Rule（严格别名规则）

通过不同类型的指针访问同一内存是 UB（除非满足特定例外）：

```cpp
float f = 3.14f;
int* p = reinterpret_cast<int*>(&f);
int i = *p;  // UB! 违反 strict aliasing

// 正确方式：
std::memcpy(&i, &f, sizeof(i));  // OK，memcpy 是允许的
// 或使用 std::bit_cast (C++20)
int i2 = std::bit_cast<int>(f);  // OK
```

**例外**：`char*`、`unsigned char*`、`std::byte*` 可以访问任何对象的底层表示。

## Placement New

在已分配的内存上构造对象：

```cpp
alignas(int) char buf[sizeof(int)];  // 手动分配内存
int* p = new (buf) int(42);          // 在 buf 上构造 int
p->~int();                            // 必须手动析构
// 不需要 delete——buf 是栈上数组
```

## std::launder（C++17）

在某些情况下，对象的地址可能因为类型变化而"失效"。`std::launder` 告诉编译器"这个指针确实指向一个活的对象"：

```cpp
struct X { const int n; };
X* p = new X{42};

// 在 p 的内存上重新构造 X（合法的，因为 const 成员相同）
p->~X();
X* p2 = new (p) X{43};

// p2 和 p 指向同一地址，但编译器可能缓存了 p->n 的值
// std::launder 告诉编译器不要使用缓存
int val = std::launder(p2)->n;  // OK: 43
```

## Object Representation（对象表示）

对象在内存中的实际字节序列。`sizeof(T)` 返回对象表示的字节数。但对象表示可能包含 **padding**（填充字节）：

```cpp
struct S {
  char a;    // 1 字节
  // 3 字节 padding
  int b;     // 4 字节
  char c;    // 1 字节
  // 3 字节 padding
};
// sizeof(S) == 12（不是 6！因为对齐要求）
```
