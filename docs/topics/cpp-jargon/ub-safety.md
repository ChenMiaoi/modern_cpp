# 未定义行为与安全术语

## Undefined Behavior（UB，未定义行为）

标准不规定程序行为——编译器可以做任何事。UB 不是"崩溃"，而是"任何事都可能发生"。

```cpp
int a[3] = {1, 2, 3};
int x = a[5];  // UB：越界访问
// 可能返回垃圾值、可能崩溃、可能"正常工作"然后在关键时刻爆炸
// 编译器甚至可以假设 UB 不会发生并据此优化——删除相关代码路径
```

### 为什么 UB 存在？

1. **性能**：不检查越界、不检查溢出 → 更快的代码
2. **可移植性**：不同平台的行为不同，标准不强制统一
3. **优化机会**：编译器可以假设 UB 不发生 → 激进优化

## Implementation-Defined Behavior（实现定义行为）

标准要求行为有定义，但具体行为由编译器决定。编译器**必须文档化**其行为。

```cpp
sizeof(int);           // 实现定义（通常是 4）
int i = -1;
unsigned u = i;        // 实现定义（通常是 2^32 - 1）
```

## Unspecified Behavior（未指明行为）

标准规定了允许的行为范围，但不指定具体是哪一个。编译器不需要文档化。

```cpp
int f() { return 1; }
int g() { return 2; }
int x = f() + g();  // f() 和 g() 的调用顺序未指明
// 可能先调用 f()，也可能先调用 g()
```

## 常见 UB 列表

### Signed Overflow（有符号溢出）

```cpp
int x = INT_MAX;
x++;  // UB! 有符号整数溢出是 UB

unsigned y = UINT_MAX;
y++;  // OK! 无符号整数溢出是 well-defined（模 2^N）
```

### Null Dereference（空指针解引用）

```cpp
int* p = nullptr;
*p = 42;  // UB
```

### Use-After-Free（释放后使用）

```cpp
int* p = new int(42);
delete p;
*p = 0;  // UB：p 指向的内存已释放
```

### Buffer Overflow（缓冲区溢出）

```cpp
int arr[10];
arr[10] = 42;  // UB：下标越界（有效下标是 0-9）
```

### Dangling Reference（悬垂引用）

```cpp
int& bad() {
  int x = 42;
  return x;  // 返回局部变量的引用——UB
}
```

### Strict Aliasing Violation

```cpp
float f = 3.14f;
int i = *reinterpret_cast<int*>(&f);  // UB! 违反严格别名规则
```

### 未初始化变量使用

```cpp
int x;
int y = x + 1;  // UB：x 未初始化
```

## Nasal Demons

UB 的经典比喻——"任何事都可能发生，包括让恶魔从你鼻子里飞出来"。这是 comp.lang.c 时代的玩笑话，但精确地描述了 UB 的危险性。

## Sanitizers（消毒器）

编译器工具，运行时检测 UB：

```bash
# AddressSanitizer (ASan)：检测内存错误
g++ -fsanitize=address main.cpp && ./a.out

# ThreadSanitizer (TSan)：检测数据竞争
g++ -fsanitize=thread main.cpp && ./a.out

# UndefinedBehaviorSanitizer (UBSan)：检测 UB
g++ -fsanitize=undefined main.cpp && ./a.out

# MemorySanitizer (MSan)：检测未初始化内存使用
clang++ -fsanitize=memory main.cpp && ./a.out
```

## Contracts（C++26 提案）

在函数前置/后置条件中显式检查——将隐式的 UB 变为显式的运行时错误：

```cpp
int divide(int a, int b)
  pre (b != 0)              // 前置条件：b 不为零
  post (r: r * b == a)      // 后置条件：结果正确
{
  return a / b;
}
```

如果违反 contracts，默认行为是 `std::terminate`——比 UB 安全得多。
