---
title: "C++ 异常处理 ABI 深度解析"
topic: topics
feature: exception-abi
status_checked_at: 2026-06-02
standard: N/A
---

# C++ 异常处理 ABI 深度解析

> 异常处理是 C++ 中唯一一个"正常路径零开销、异常路径高开销"的错误传播机制。理解其 ABI 层面的实现，是写出既正确又高效的 C++ 代码的前提——你必须知道 `noexcept` 到底省了什么、`try` 块到底增了什么、以及为什么"零成本异常"在不抛出时真的零成本。

---

## 一、Itanium C++ ABI 异常处理模型

C++ 异常处理的 ABI 规范来自 [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html)，被 GCC、Clang、ICC 等主流编译器采纳（MSVC 使用自己的 SEH 模型，原理类似但实现不同）。其核心设计目标：

1. **零运行时开销**：不抛出异常时，正常路径没有任何额外指令（无检查、无设置）
2. **表驱动（table-driven）**：异常处理的元数据存储在只读段中，不嵌入代码段
3. **语言无关**：personality routine 机制允许 C++、Java、Rust 等语言共享同一套栈展开基础设施

```
┌─────────────────────────────────────────────────────────────┐
│                Itanium ABI 异常处理架构                       │
│                                                             │
│  ┌──────────┐    throw     ┌──────────────┐    unwind       │
│  │ 用户代码  │ ──────────→ │  __cxa_throw  │ ──────────→     │
│  │ throw X; │              │  分配异常对象  │   _Unwind_*     │
│  └──────────┘              └──────────────┘                 │
│       │                         │                           │
│       │ catch                   │ personality               │
│       ▼                         ▼                           │
│  ┌──────────┐              ┌──────────────┐                 │
│  │ landing  │ ◄─────────── │  .eh_frame   │                 │
│  │ pad      │   查 LSDA    │  .gcc_except │                 │
│  └──────────┘              │  _table      │                 │
│       │                    └──────────────┘                 │
│       │ __cxa_begin_catch                                    │
│       │ __cxa_end_catch                                      │
│       ▼                                                      │
│  ┌──────────┐                                                │
│  │ catch 处理│                                                │
│  │ 逻辑     │                                                │
│  └──────────┘                                                │
└─────────────────────────────────────────────────────────────┘
```

### 关键运行时函数

异常处理涉及以下 C++ ABI 运行时函数（定义在 `libc++abi` 或 `libstdc++` 中）：

| 函数 | 职责 |
|------|------|
| `__cxa_allocate_exception` | 在堆上分配异常对象（含 `__cxa_exception` 头部） |
| `__cxa_throw` | 初始化异常对象，调用 `_Unwind_RaiseException` |
| `__cxa_begin_catch` | 标记当前异常正在被处理，增加处理计数 |
| `__cxa_end_catch` | 减少处理计数，若为 0 则析构并释放异常对象 |
| `__cxa_rethrow` | 重新抛出当前正在处理的异常 |
| `__cxa_current_primary_exception` | 获取当前异常的 `__cxa_exception*` 指针 |
| `__cxa_get_exception_ptr` | 获取异常对象的指针（用于 `catch(...)` 中的 rethrow） |

---

## 二、异常抛出与捕获的完整流程

### 2.1 `__cxa_throw` 的内部结构

当编译器遇到 `throw expr;` 时，生成的代码大致等价于：

```cpp
// 编译器为 throw 42; 生成的伪代码
void* exn_obj = __cxa_allocate_exception(sizeof(int));
new (exn_obj) int(42);                        // 在异常对象中构造值
__cxa_throw(exn_obj,                          // 异常对象指针
            &typeid(int),                     // type_info 指针
            (void(*)(void*))&int::~int);      // 析构函数指针（POD 为 nullptr）
```

`__cxa_throw` 内部做的事情：

```
__cxa_allocate_exception(size):
  1. 分配 sizeof(__cxa_exception) + size 的内存
  2. 返回指向有效载荷（payload）的指针（跳过头部）
  3. 头部结构对用户不可见

__cxa_throw(exn_obj, tinfo, dest):
  1. 初始化 __cxa_exception 头部字段：
     - exceptionType    = tinfo
     - exceptionDestructor = dest
     - refcount         = 1
     - handlerCount     = 0
     - propagating      = 0
  2. 调用 _Unwind_RaiseException(&exception_header)
  3. 如果 _Unwind_RaiseException 返回（即没有找到 catch）：
     - 调用 std::terminate()
```

### 2.2 `__cxa_exception` 头部布局

异常对象在内存中的布局：

```
低地址                                                        高地址
┌────────────────────────────────┬───────────────────────────┐
│    __cxa_exception 头部        │    异常对象（用户数据）     │
│    (64 字节, x86-64)           │    sizeof(T)               │
│                                │                            │
│  ┌──────────────────────────┐  │                            │
│  │ refcount                 │  │                            │
│  │ exceptionType            │  │  ← __cxa_throw 返回这里    │
│  │ exceptionDestructor      │  │    （对用户可见的指针）      │
│  │ unexpectedHandler        │  │                            │
│  │ terminateHandler         │  │                            │
│  │ nextException            │  │                            │
│  │ handlerCount             │  │                            │
│  │ propagating              │  │                            │
│  │ ...                      │  │                            │
│  └──────────────────────────┘  │                            │
└────────────────────────────────┴───────────────────────────┘
                                  ↑
                          用户代码看到的指针
```

`_Unwind_Exception` 结构体嵌入在 `__cxa_exception` 的开头：

```cpp
struct _Unwind_Exception {
    uint64_t exception_class;    // 4字节标识，如 "GNUC" 或 "CLNG"
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    _Unwind_Word private_1;      // 展开器内部使用
    _Unwind_Word private_2;      // 展开器内部使用
};

struct __cxa_exception {
    std::type_info* exceptionType;
    void (*exceptionDestructor)(void*);
    std::unexpected_handler unexpectedHandler;
    std::terminate_handler terminateHandler;
    __cxa_exception* nextException;
    int handlerCount;
    int propagating;             // C++11 起用于标记嵌套异常
    _Unwind_Exception unwindHeader;
    // ... 后面是用户异常对象
};
```

### 2.3 `__cxa_begin_catch` / `__cxa_end_catch`

```
__cxa_begin_catch(unwind_header):
  1. 从 unwind_header 定位 __cxa_exception 头部
  2. handlerCount++（允许多个 catch 子句共享同一异常）
  3. 将异常从"传播中"链表移除
  4. 返回异常对象指针

__cxa_end_catch():
  1. handlerCount--
  2. 若 handlerCount == 0（最后一个 catch 块结束）：
     - 调用 exceptionDestructor 析构异常对象
     - __cxa_free_exception 释放内存
  3. 恢复上一个异常的上下文（嵌套异常支持）
```

**嵌套异常处理**：每个线程维护一个"正在处理的异常"栈。当 `catch` 块中又抛出新异常时，新异常链接到当前异常的链表上。`__cxa_end_catch` 在清理时检查是否有传播中的异常。

### 2.4 `rethrow` 语义

```cpp
try {
    throw 42;
} catch (int& e) {
    throw;  // rethrow —— 重新抛出同一个异常对象
}
```

`throw;`（不带表达式）编译为 `__cxa_rethrow()`：

```
__cxa_rethrow():
  1. 从线程局部状态获取当前正在处理的异常
  2. 标记 propagating = 1（告知展开器不要销毁此异常）
  3. 调用 _Unwind_RaiseException
  4. 若未被 catch → std::terminate()
```

关键区别：`rethrow` 不会重新分配异常对象，而是传播同一个对象。`handlerCount` 增加，`__cxa_end_catch` 不会释放，直到最后一次 catch 结束。

```cpp
void process() {
    try {
        throw std::runtime_error("oops");
    } catch (std::exception& e) {
        try {
            // do some cleanup...
            throw;  // 同一个对象，不拷贝
        } catch (...) {
            // handlerCount == 2
            throw;  // 继续传播
        }
        // __cxa_end_catch 在 catch 块结束时调用，但 handlerCount > 0，不释放
    }
    // 这里 __cxa_end_catch handlerCount 降为 0，释放异常对象
}
```

---

## 三、栈展开机制（Stack Unwinding）

### 3.1 展开流程

当 `__cxa_throw` 调用 `_Unwind_RaiseException` 时，展开器开始两阶段搜索：

```
_Unwind_RaiseException(exception_header):

  ═══════════════════════════════════════
  Phase 1: 搜索阶段（Search Phase）
  ═══════════════════════════════════════
  从 throw 点开始，沿调用栈向上遍历每一帧：
    1. 读取该帧的 .eh_frame 中的 FDE（Frame Description Entry）
    2. 如果 FDE 有 personality routine：
       调用 personality(phase=search, actions=0)
       - personality 读取 LSDA
       - 检查当前 PC 是否在某个 catch 区间内
       - 检查 catch 的类型是否匹配
       若匹配 → 返回 _URC_HANDLER_FOUND，结束搜索
    3. 若不匹配，移动到上一帧，继续搜索
  若遍历完整个栈都没找到匹配 → 调用 std::terminate()

  ═══════════════════════════════════════
  Phase 2: 清理阶段（Cleanup Phase）
  ═══════════════════════════════════════
  再次从 throw 点开始向上遍历：
    1. 对每一帧：
       调用 personality(phase=unwind, actions=UA_CLEANUP_PHASE)
       - personality 返回 _URC_CONTINUE_UNWIND
       - 调用该帧的 cleanup action（析构局部变量）
    2. 到达找到 catch 的那一帧时：
       personality 返回 _URC_INSTALL_CONTEXT
       - 恢复寄存器上下文
       - 跳转到 landing pad 地址
       - 异常被 transfer 到 catch 块
```

**为什么是两阶段？** Phase 1 快速扫描不修改任何状态（不调用析构函数），如果整个栈没有匹配的 catch，可以直接 `terminate()`。Phase 2 真正执行析构和控制转移。如果只用一阶段，析构了部分对象后发现没有 catch，程序状态已经被破坏了。

### 3.2 Personality Routine

Personality routine 是每个语言（甚至每个函数）提供的回调函数，由展开器在每帧调用。C++ 使用 `__gxx_personality_v0`（GCC）或 `__clang_call_terminate`（Clang 内部使用 `__gxx_personality_v0`）。

```cpp
// personality routine 的签名（简化）
_Unwind_Reason_Code __gxx_personality_v0(
    int version,
    _Unwind_Action actions,
    uint64_t exception_class,
    _Unwind_Exception* unwind_header,
    _Unwind_Context* context    // 展开器提供的栈帧上下文
);
```

Personality routine 内部的工作：

```
1. 从 context 中读取当前函数的 LSDA（Language Specific Data Area）
2. 从 LSDA 中解析：
   - call site table：哪些指令范围可以抛出
   - action table：每个 call site 对应的类型过滤器
   - type table：catch 的 type_info 指针数组
3. 根据当前 PC 找到对应的 call site entry
4. 若 actions 包含 UA_SEARCH_PHASE：
   - 遍历 action chain，对每个 type_info 调用 __dynamic_cast 检查类型匹配
   - 匹配成功 → 返回 _URC_HANDLER_FOUND
   - 无匹配 → 返回 _URC_CONTINUE_UNWIND
5. 若 actions 包含 UA_CLEANUP_PHASE：
   - 若当前 PC 对应的 catch 为目标帧 → 设置 context 的 PC 为 landing pad
     → 返回 _URC_INSTALL_CONTEXT
   - 若是清理（destructor）→ 设置 cleanup action → 返回 _URC_INSTALL_CONTEXT
   - 否则 → 返回 _URC_CONTINUE_UNWIND
```

### 3.3 类型匹配机制

C++ 的 catch 匹配规则在 ABI 层通过 `__dynamic_cast` 实现：

```
catch (Base& e)  是否能匹配 throw Derived()？

  type_info 比较：
  ┌────────────────────────────────────────────────────┐
  │ 对于 catch 的 type_info 和异常的 type_info：         │
  │                                                    │
  │ 1. 完全相等 → 直接匹配                              │
  │ 2. catch 是 public 基类 → __dynamic_cast 检查继承链 │
  │ 3. catch 是 void* → 匹配所有异常                    │
  │ 4. catch 是 ... → 匹配所有异常                      │
  │ 5. catch 是指针类型 → 类似但用指针类型匹配           │
  │                                                    │
  │ 注意：异常对象本身不是 polymorphic 的！               │
  │ 类型匹配完全由 type_info + __dynamic_cast 完成      │
  └────────────────────────────────────────────────────┘
```

---

## 四、`.eh_frame` 与 `.gcc_except_table` 段

### 4.1 `.eh_frame` 段

`.eh_frame` 使用 **DWARF CFI（Call Frame Information）** 格式，描述如何展开每个栈帧。它在每个函数编译时生成，链接时合并到最终的 `.eh_frame` 段。

```
.eh_frame 段结构：
┌───────────────────────────────────────────────────────────┐
│ CIE (Common Information Entry)                            │
│   - version, augmentation, code/data alignment factor     │
│   - 返回地址寄存器编号                                     │
│   - 初始指令（所有 FDE 共享的寄存器保存规则基线）          │
├───────────────────────────────────────────────────────────┤
│ FDE (Frame Description Entry) — 每个函数一个               │
│   - CIE 指针                                              │
│   - 函数起始地址 (PC_begin)                               │
│   - 函数长度 (PC_range)                                   │
│   - 该函数的 CFI 指令序列                                 │
│     描述：每个指令位置的寄存器保存在哪里                    │
│           如何计算 CFA（Canonical Frame Address）          │
│   - LSDA 指针（如果有异常处理）                           │
├───────────────────────────────────────────────────────────┤
│ FDE — 另一个函数                                          │
│   ...                                                     │
├───────────────────────────────────────────────────────────┤
│ .eh_frame_hdr（可选，加速查找）                           │
│   - 二分查找表：PC → FDE 映射                             │
└───────────────────────────────────────────────────────────┘
```

CFI 指令示例：

```
# 函数 prologue 的 CFI 注解（编译器自动生成）
.cfi_startproc                    # FDE 开始
.cfi_def_cfa_offset 16            # CFA = RSP + 16
.cfi_offset 6, -16                # RBP 保存在 [CFA-16]
pushq %rbp                        # push rbp
.cfi_def_cfa_offset 24            # CFA = RSP + 24
.cfi_offset 6, -24                # RBP 保存位置更新
movq %rsp, %rbp
.cfi_def_cfa_register 6           # CFA = RBP
...
.cfi_endproc                      # FDE 结束
```

展开器使用这些信息，在任意指令位置都能：
1. 计算 CFA（调用者的栈指针）
2. 找到每个被保存寄存器的恢复位置
3. 恢复调用者的寄存器集

### 4.2 `.gcc_except_table`（LSDA 段）

`.gcc_except_table` 包含每个需要异常处理的函数的 **LSDA（Language Specific Data Area）**。Personality routine 读取 LSDA 来决定如何处理当前帧的异常。

```
.gcc_except_table 结构：
┌───────────────────────────────────────────────────────────┐
│ LSDA Header                                               │
│   - LPStart: landing pad 基址（通常是函数起始地址）       │
│   - TTBase: type table 基址偏移                           │
│   - Call site table encoding                              │
│   - Call site table length                                │
├───────────────────────────────────────────────────────────┤
│ Call Site Table                                           │
│   每个 entry 描述一个可能抛出的代码区间：                   │
│   ┌──────────┬──────────┬──────────┬──────────┐           │
│   │ CS_start │ CS_len   │ CS_lp    │ CS_action│           │
│   │ 起始偏移 │ 长度     │ LP偏移   │ action   │           │
│   │          │          │ (0=无)   │ 索引     │           │
│   └──────────┴──────────┴──────────┴──────────┘           │
├───────────────────────────────────────────────────────────┤
│ Action Table                                              │
│   链表结构，每个 entry：                                    │
│   ┌──────────┬──────────┬──────────┐                      │
│   │ AR_filter │ AR_disp  │ AR_next  │                      │
│   │ type表索引│ 下一action│ 链表指针 │                      │
│   │ (0=清理)  │ 偏移    │ (0=结束) │                      │
│   └──────────┴──────────┴──────────┘                      │
├───────────────────────────────────────────────────────────┤
│ Type Table                                                │
│   type_info* 数组（倒序排列，索引 1 = 第一个 catch 类型）   │
│   [0] = nullptr（保留）                                    │
│   [1] = &typeid(int)                                      │
│   [2] = &typeid(std::exception)                           │
│   ...                                                     │
└───────────────────────────────────────────────────────────┘
```

### 4.3 一个完整示例

```cpp
void foo() {
    Widget w;              // 需要析构
    try {
        bar();             // 可能抛出
        baz();             // 可能抛出
    } catch (int e) {
        handle_int(e);
    } catch (const std::exception& e) {
        handle_exn(e);
    }
    // w 的析构在函数结束时
}
```

生成的 LSDA（概念性表示）：

```
Call Site Table:
  ┌────────────┬────────────┬────────────┬───────────┐
  │ CS_start   │ CS_len     │ CS_lp      │ CS_action │
  ├────────────┼────────────┼────────────┼───────────┤
  │ bar 调用处 │ bar 调用长度│ LP1        │ 1         │
  │ baz 调用处 │ baz 调用长度│ LP1        │ 1         │
  └────────────┴────────────┴────────────┴───────────┘
  LP1 指向 catch 入口

Action Table:
  Action 1: AR_filter=1, AR_disp=sizeof(Action), AR_next=2
  Action 2: AR_filter=2, AR_disp=0,            AR_next=0

Type Table:
  [1] = &typeid(int)
  [2] = &typeid(std::exception)

匹配逻辑（personality routine）：
  1. 异常类型 = typeid(Widget) 或其他
  2. 先检查 Action 1 (int) → 不匹配
  3. 沿链表到 Action 2 (std::exception) → 检查 __dynamic_cast
  4. 如果都不匹配 → 传播到上一帧
```

---

## 五、零成本异常模型（Table-Driven）

### 5.1 为什么叫"零成本"

"零成本"指的是**不抛出异常时的运行时成本为零**——不是说抛出时免费，恰恰相反：

```
对比两种异常实现模型：

┌──────────────────────────────────────────────────────────────┐
│ Model 1: Dwarfwinding (Itanium ABI / GCC / Clang 默认)       │
│                                                              │
│ 正常路径：零指令开销                                          │
│   - 无 setjmp, 无状态检查                                    │
│   - 展开信息完全在 .eh_frame / .gcc_except_table 段中        │
│   - 只在发生 throw 时读取这些表                               │
│                                                              │
│ 异常路径：高成本                                              │
│   - 两阶段栈展开（遍历整个调用栈两次）                        │
│   - 每帧调用 personality routine                             │
│   - type_info 比较（可能涉及字符串比较或 dynamic_cast）      │
│   - 表查找开销（线性或二分搜索）                              │
│                                                              │
│ 代价：代码体积增大（异常表 + landing pad），ICache 压力      │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ Model 2: SJLJ (SetJump/LongJump，某些嵌入式/旧系统)          │
│                                                              │
│ 正常路径：每个 try 块 + setjmp（约 10-20 指令）              │
│   - 每次进入 try 块都调用 setjmp 保存寄存器                  │
│   - 空间：每个 try 块约 200 字节 jmp_buf                     │
│                                                              │
│ 异常路径：相对快                                              │
│   - longjmp 跳转到 setjmp 点，仅需一次                       │
│   - 但需要手动调用沿途析构函数（如果有的话）                  │
│                                                              │
│ 代价：正常路径慢 ~10-20 指令/try 块                          │
└──────────────────────────────────────────────────────────────┘
```

**选择哪一种？** 几乎所有现代编译器都使用表驱动（dwarf）模型，因为"不抛出"是绝大多数代码的常态。SJLJ 仅在目标平台不支持 DWARF 展开时使用（如某些嵌入式 ARM、iOS 32-bit）。

GCC 可以显式选择：
```bash
gcc -fexceptions -fsjlj-exceptions    # 强制使用 SJLJ
gcc -fexceptions -fnon-call-exceptions # 允许非调用点的异常（如信号）
```

### 5.2 代码体积代价

零成本异常不是免费的——它用磁盘空间换取运行时零开销：

```
源码中每个 try-catch 块和每个可能抛出的函数调用都会产生：

1. .eh_frame 中的 FDE 条目
   - 通常 32-128 字节/函数
   - 即使没有异常处理，为了 debugger 回溯也需要

2. .gcc_except_table 中的 LSDA
   - Call site table：每个可能抛出的调用点 ~8-16 字节
   - Action table：每个 catch 类型 ~8 字节
   - Type table：每个 catch 的 type_info* ~8 字节

3. 代码段中的 landing pad
   - catch 块的代码（可能被放入冷路径）
   - cleanup 代码（析构调用）

实际数据（典型 C++ 程序）：
  .eh_frame         ~5-15% 的二进制体积
  .gcc_except_table ~1-5% 的二进制体积
  landing pads      ~1-3% 的代码段
```

---

## 六、`noexcept` 的实现原理

### 6.1 编译器如何处理 `noexcept`

`noexcept` 在 ABI 层面的效果是**不为该函数生成 landing pad 和对应的 LSDA 条目**：

```cpp
void safe() noexcept {
    // 编译器知道此函数承诺不抛出
    // 不生成任何异常表条目
}

void unsafe() {
    // 编译器为每个可能抛出的调用点生成 call site table entry
    // 为 catch 块生成 landing pad
}
```

编译器为 `noexcept` 函数生成的代码：

```
safe() noexcept:
  ; 正常代码...
  ; 如果内部有 throw → 编译器直接生成 __cxa_call_terminate
  ; 而不是通过栈展开机制
  ; 不需要 .gcc_except_table 中的 LSDA

unsafe():
  ; 正常代码...
  ; 每个可能抛出的调用点后面有 .gcc_except_table 条目
  ; 如果有 try-catch，有对应的 landing pad
```

**`noexcept` 省了什么？**
1. 不在 `.gcc_except_table` 中生成 call site 条目（减少表体积）
2. 允许更激进的代码移动（编译器知道不会被异常中断）
3. 减少 landing pad 代码（冷路径不膨胀）

### 6.2 `noexcept` 与 `__cxa_call_terminate`

当 `noexcept` 函数真的抛出异常时：

```cpp
void f() noexcept {
    throw 42;  // UB！但编译器会插入兜底
}
```

编译器在 `noexcept` 函数的 **每个可能抛出的调用点** 插入检查：

```
# f() 的生成代码（简化）
f:
    call some_throwing_function
    # 如果 some_throwing_function 正常返回，继续
    ret

# 但是！编译器还会在 unwind tables 中标记此函数为 noexcept
# 展开器在 Phase 2 遇到 noexcept 帧时：
#   personality routine 返回 _URC_FATAL_PHASE1_ERROR
#   → __cxa_call_terminate() → std::terminate()
```

实际上，GCC/Clang 不在每个调用点插入显式检查。而是依赖展开器在 unwinding 过程中发现 noexcept 帧时的处理：personality routine 查看 LSDA 发现函数被标记为 noexcept，直接返回错误码，展开器调用 `std::terminate()`。

### 6.3 条件 `noexcept`

```cpp
void maybe_throw() noexcept(sizeof(T) <= 64) {
    // 如果 T 满足条件 → noexcept
    // 否则 → 允许异常传播
    // 编译器为整个函数选择一种模式
}
```

条件 `noexcept` 在编译期求值，结果为 `true` 时等同于 `noexcept`，为 `false` 时等同于无异常规格。标准库大量使用：

```cpp
// std::vector 的移动构造函数
vector(vector&& other) noexcept;  // 移动语义保证不抛

// std::swap 的默认实现
template <typename T>
void swap(T& a, T& b) noexcept(noexcept(T(std::move(a))) &&
                                 noexcept(a.operator=(std::move(b))));
```

---

## 七、异常对象的内存管理

### 7.1 分配策略

```
__cxa_allocate_exception(size):
  ┌──────────────────────────────────────────────────┐
  │ 默认策略：malloc(size + sizeof(__cxa_exception)) │
  │                                                  │
  │ 这意味着 throw std::string("hello") 需要：       │
  │   sizeof(__cxa_exception) ≈ 120 字节（x86-64）   │
  │ + sizeof(std::string) ≈ 32 字节                  │
  │ + 堆上字符串数据（如果超过 SSO 阈值）             │
  │                                                  │
  │ 总计：~152+ 字节堆分配                            │
  │                                                  │
  │ ⚠️ 如果 malloc 失败 → std::terminate()           │
  │ （不能用异常来处理异常分配失败——逻辑上无解）      │
  └──────────────────────────────────────────────────┘
```

### 7.2 异常对象的引用计数

`__cxa_exception` 头部的 `refcount` 字段管理共享：

```
场景 1: 正常 catch — refcount = 1
  throw → catch → end_catch → refcount=0 → free

场景 2: rethrow — refcount 增加
  throw(1) → catch → rethrow(2) → catch → end_catch(2→1)
                                   → end_catch(1→0) → free

场景 3: std::exception_ptr — refcount 增加
  throw(1) → catch → current_exception(refcount++) → end_catch(1→1)
  // exception_ptr 持有引用，异常对象不释放
  // exception_ptr 析构时 refcount-- → 0 → free
```

```cpp
std::exception_ptr ep;
try {
    throw std::runtime_error("oops");
} catch (...) {
    ep = std::current_exception();  // refcount++
}  // end_catch: refcount 1→1 (不释放)
// ep 析构时: refcount 1→0 → 释放异常对象
```

---

## 八、异常规格（Exception Specification）

### 8.1 动态异常规格（C++17 起已移除）

```cpp
// C++98/03 的异常规格——已经被废弃并移除
void f() throw(int, std::exception);  // 只允许抛出 int 或 std::exception
void g() throw();                      // 不允许抛出任何异常（等价于 noexcept）

// C++17 起：throw() 被等价于 noexcept
// throw(Type...) 被完全移除
```

动态异常规格的实现方式：编译器在每个 throw 点插入类型检查，如果抛出的类型不在允许列表中，调用 `std::unexpected()`。这带来运行时开销，且与模板交互时语义混乱。

### 8.2 `noexcept` 替代动态异常规格

```
动态异常规格（废弃）：          noexcept（现代）：
  - 运行时检查                    - 编译期约束
  - 每个 throw 点有开销           - 不生成异常表条目
  - 违反时调用 unexpected()       - 违反时调用 terminate()
  - 与模板交互困难                - 条件 noexcept 与模板完美配合
  - 已从 C++17 标准中移除         - 是唯一的异常规格机制
```

---

## 九、`-fno-exceptions` 的影响

### 9.1 禁用异常后的编译器行为

```bash
g++ -fno-exceptions program.cpp
```

```
-fno-exceptions 的效果：

┌────────────────────────────────────────────────────────────┐
│ 1. throw 语句 → 编译错误（不是运行时行为改变）              │
│    error: exception handling disabled, use '-fexceptions'  │
│                                                            │
│ 2. try / catch → 编译错误                                  │
│                                                            │
│ 3. 标准库中使用异常的代码：                                │
│    - 不生成异常表                                          │
│    - 不调用 __cxa_throw                                    │
│    - __throw_* 函数变为 no-op 或直接调用 std::abort        │
│                                                            │
│ 4. .eh_frame 段仍然生成（debugger 需要回溯信息）           │
│    但 .gcc_except_table 基本为空                           │
│                                                            │
│ 5. landing pad 代码不生成 → 代码段减小                     │
└────────────────────────────────────────────────────────────┘
```

### 9.2 `-fno-exceptions` 与标准库的兼容性问题

```cpp
std::vector<int> v;
v.at(5);  // at() 抛出 std::out_of_range
          // 在 -fno-exceptions 下：调用 std::abort()
          // 而不是抛出——你的程序直接崩溃
```

标准库函数不是 `noexcept` 的（如 `std::vector::at`、`std::stoi`、容器的 `push_back` 等），在 `-fno-exceptions` 下的行为**未被标准定义**——实际上是实现定义的。GCC 的 libstdc++ 在这种模式下将 `__throw_*` 函数替换为 `std::abort()`。

### 9.3 何时使用 `-fno-exceptions`

```
适合：
  - 嵌入式系统（无堆空间分配异常对象）
  - 实时系统（异常展开延迟不可预测）
  - GPU 代码（不支持异常）
  - 与 C 接口层（C 不理解异常）

不适合：
  - 使用标准库的通用代码
  - 需要 RAII 的代码（析构函数依赖栈展开）
  - 库代码（无法要求用户也关闭异常）
```

**现代替代方案**：使用 `noexcept` 标注不抛出的函数，用 `std::expected`（C++23）或 `std::variant` 做错误传播，而不是全局关闭异常。

---

## 十、异常安全保证

### 10.1 三个保证等级

```
┌─────────────────────────────────────────────────────────────┐
│ Nothrow Guarantee（不抛异常保证）                            │
│                                                             │
│ 操作承诺不抛出任何异常。                                     │
│ 所有析构函数、移动操作、swap 必须满足此保证。               │
│                                                             │
│ void swap(T& a, T& b) noexcept;  // 必须                     │
│ ~T() noexcept;                    // C++11 起隐式 noexcept  │
│ T(T&&) noexcept;                  // 移动构造               │
│ T& operator=(T&&) noexcept;       // 移动赋值               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Strong Guarantee（强异常保证）                               │
│                                                             │
│ 如果操作抛出异常，程序状态回滚到操作开始前。                  │
│ 要么操作完全成功，要么状态不变。                              │
│                                                             │
│ std::vector::push_back 的策略：                              │
│   1. 如果需要扩容：先分配新内存（可能抛）                    │
│   2. 拷贝/移动元素到新内存（可能抛）                         │
│   3. 释放旧内存                                              │
│   如果步骤 1 或 2 抛出 → vector 保持原样（strong）          │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Basic Guarantee（基本异常保证）                              │
│                                                             │
│ 如果操作抛出异常：                                           │
│   - 没有资源泄漏（RAII 保证）                               │
│   - 所有对象仍处于有效状态（但不确定是哪个状态）            │
│   - 可以安全地销毁或重新赋值                                 │
│                                                             │
│ std::vector::operator= 的 copy-and-swap 实现：              │
│   Widget& operator=(Widget other) {  // 按值传参，可能抛     │
│       swap(*this, other);            // noexcept            │
│       return *this;                  // other 析构，安全     │
│   }                                                          │
│   如果构造 other 时抛出 → this 不变（strong）              │
│   如果实现不是 copy-and-swap → 可能只有 basic              │
└─────────────────────────────────────────────────────────────┘
```

### 10.2 编写异常安全代码的关键模式

```cpp
// 模式 1: 先准备，后 commit（strong guarantee）
void ResourceManager::addResource(Resource r) {
    // Step 1: 准备新状态（可能抛出）
    auto new_vec = resources_;        // 拷贝（可能抛）
    new_vec.push_back(std::move(r));  // 可能抛（如果需要扩容且 move 抛）

    // Step 2: Commit（noexcept）
    resources_ = std::move(new_vec);  // 移动赋值 noexcept
}

// 模式 2: RAII + 明确的 noexcept 标注
class Database {
    Connection conn_;
    std::mutex mtx_;
public:
    // 析构、移动、swap 都是 noexcept
    ~Database() noexcept { conn_.close(); }
    Database(Database&& o) noexcept : conn_(std::move(o.conn_)) {}
    void swap(Database& o) noexcept {
        using std::swap;
        swap(conn_, o.conn_);
    }
};
```

---

## 十一、性能成本分析

### 11.1 正常路径的成本

```
表驱动异常的正常路径成本：

┌────────────────────────────────────────────────────────────┐
│ 直接运行时成本：零                                          │
│   - 不检查"是否在 try 块中"                                │
│   - 不调用 setjmp/longjmp                                  │
│   - 不维护异常状态链表                                      │
│                                                            │
│ 间接成本（非常显著）：                                      │
│                                                            │
│ 1. 代码体积膨胀（ICache 压力）                              │
│    - landing pad 代码：~1-5% 的代码段                       │
│    - 但这些代码是冷路径，通常在独立的 .text 段              │
│    - 编译器会把 catch 代码放到函数末尾或独立段              │
│                                                            │
│    GCC: -ffunction-sections + -fdata-sections               │
│    链接器: --gc-sections 可以删除未使用的 landing pad       │
│                                                            │
│ 2. .eh_frame 段体积                                         │
│    - 每个函数都有 FDE 条目（用于调试器回溯）                │
│    - 典型程序中 5-15% 的二进制体积                          │
│    - 可以用 -fno-asynchronous-unwind-tables 减小            │
│      （但会破坏 backtrace 和调试）                          │
│                                                            │
│ 3. 编译时间增加                                             │
│    - 生成异常表需要分析每个函数的控制流                     │
│    - 确定哪些调用点可能抛出                                 │
│    - 生成 landing pad 的控制流图                            │
│                                                            │
│ 4. 链接时间增加                                             │
│    - 合并多个 .eh_frame 段                                  │
│    - 生成 .eh_frame_hdr（二分查找表）                       │
└────────────────────────────────────────────────────────────┘
```

### 11.2 异常路径的成本

```
抛出一个异常的开销：

__cxa_allocate_exception:  ~1 次 malloc（100-200 字节）
_Unwind_RaiseException:
  Phase 1 (search):
    - 遍历每一帧：读 .eh_frame → 解析 FDE → 调用 personality
    - 每帧约 ~1-5 μs（取决于 LSDA 复杂度）
    - 如果调用栈深度 N，约 N × 2-5 μs
  Phase 2 (cleanup):
    - 再次遍历，逐帧清理
    - 调用每个局部对象的析构函数
    - 每帧约 ~2-10 μs（加上析构函数本身的时间）
  总计：~O(N) 栈深度 × 每帧展开开销

实测数据（x86-64, GCC 13, -O2）：
  调用栈深度 10:  ~5-20 μs
  调用栈深度 50:  ~25-100 μs
  调用栈深度 200: ~100-400 μs
  （不含析构函数本身的执行时间）

对比：正常函数返回 ~1 ns
  异常比正常返回慢 1000-100000 倍
  所以：异常只应用于真正异常的情况
```

### 11.3 与返回错误码的对比

```
                  异常          返回 std::expected    返回错误码
正常路径          间接 ICache     ~0-1 指令               ~0-1 指令
                  开销            （检查 optional 值）    （if 检查）
错误路径          ~10-400 μs     ~10-50 ns               ~1-5 ns
                  （栈展开）     （构造 expected）        （返回 int）
代码侵入性        无（自动传播）  每个返回点检查          每个返回点检查
类型安全          强              强                      弱（int 错误码）
可组合性          自动传播        需要 ? 或 and_then      手动传播
                （多层调用链）   链式调用
```

---

## 十二、`setjmp/longjmp` 替代方案及其缺陷

### 12.1 SJLJ 异常模型

在 C 语言中，有时用 `setjmp/longjmp` 模拟异常：

```cpp
#include <csetjmp>

static std::jmp_buf jump_buffer;
static volatile bool exception_thrown = false;

void throw_logic_error(const char* msg) {
    exception_thrown = true;
    std::longjmp(jump_buffer, 1);
}

void risky_operation() {
    // ... 可能出错
    if (error) throw_logic_error("something went wrong");
}

int main() {
    if (setjmp(jump_buffer) == 0) {
        // 正常路径
        risky_operation();
    } else {
        // 异常路径
        std::cout << "Caught error\n";
    }
}
```

### 12.2 为什么 SJLJ 比 C++ 异常差

```
┌──────────────────────────────────────────────────────────────┐
│ SJLJ vs C++ 异常：逐项对比                                    │
│                                                              │
│ 1. 正常路径开销                                               │
│    SJLJ:  setjmp 每个 try 点 ~10-20 指令 + 保存寄存器        │
│    C++:   零指令                                              │
│                                                              │
│ 2. 析构函数调用                                               │
│    SJLJ:  longjmp 跳过中间帧 → 不调用析构函数 → 资源泄漏！    │
│    C++:   自动栈展开 → 逐帧析构                               │
│    ⚠️ 这是 SJLJ 最致命的问题                                  │
│                                                              │
│ 3. 类型安全                                                   │
│    SJLJ:  无类型信息，只能用错误码                            │
│    C++:   完整的类型信息 + catch 匹配                         │
│                                                              │
│ 4. 跨语言传播                                                 │
│    SJLJ:  不能跨函数边界（setjmp 和 longjmp 必须在同一作用域）│
│    C++:   可以跨任意多层函数调用                              │
│                                                              │
│ 5. 嵌套支持                                                   │
│    SJLJ:  需要手动管理 jmp_buf 栈                             │
│    C++:   自动支持嵌套异常                                    │
│                                                              │
│ 6. 异常安全                                                   │
│    SJLJ:  longjmp 后对象处于不确定状态                        │
│    C++:   所有局部对象被正确析构                               │
│                                                              │
│ 7. 编译器优化                                                 │
│    SJLJ:  setjmp 是 opaque call → 阻碍优化                   │
│    C++:   异常路径是冷代码 → 不影响正常路径优化               │
└──────────────────────────────────────────────────────────────┘
```

`longjmp` 的毁灭性后果：

```cpp
void dangerous() {
    std::string name = "hello";   // 构造函数分配堆内存
    std::mutex mtx;                // 构造函数初始化互斥锁
    mtx.lock();                    // 获取锁
    if (error) longjmp(buf, 1);   // 跳转！
    mtx.unlock();
    // name 和 mtx 的析构函数都不会被调用
    // → 内存泄漏
    // → 互斥锁永远不释放（死锁）
}
```

---

## 十三、编译器输出分析

### 13.1 查看异常表

```bash
# 生成汇编（含 CFI 和异常表注释）
g++ -S -O2 -fexceptions example.cpp -o example.s

# 只查看 .eh_frame 段
readelf --debug-dump=frames example.o

# 查看 .gcc_except_table 段
readelf -x .gcc_except_table example.o

# 使用 llvm-readobj 查看 LSDA
llvm-readobj --unwind example.o

# 查看调用栈展开信息
objdump -W example.o   # DWARF 信息
```

### 13.2 分析一个函数的异常表

源代码：

```cpp
// example.cpp
#include <stdexcept>

int compute(int x) {
    if (x < 0) throw std::invalid_argument("negative");
    return x * 2;
}

int process(int x) {
    int result = 0;
    try {
        result = compute(x);
    } catch (const std::exception& e) {
        result = -1;
    }
    return result;
}
```

编译并分析：

```bash
g++ -O2 -S example.cpp -o example.s
```

生成的关键汇编（x86-64 简化版）：

```asm
process(int):
    # 函数入口
    .cfi_startproc
    .cfi_personality 155, __gxx_personality_v0  # personality routine
    .cfi_lsda 0x1b, .Lexception0               # LSDA 指针

    # 正常代码
    pushq   %rbx
    .cfi_def_cfa_offset 16
    .cfi_offset 3, -16
    movl    %edi, %ebx
    call    compute(int)         # ← call site entry 1

    # 正常返回路径
    movl    %ebx, %eax
    popq    %rbx
    ret

    # Landing pad（cold path，编译器放到函数末尾）
    .Lexception0:
    cmpq    $1, %rdx             # 检查异常类型
    je      .Lcatch_handler
    .cfi_def_cfa_offset 16
    call    _Unwind_Resume       # 不匹配的类型，继续展开

    .Lcatch_handler:
    # __cxa_begin_catch
    movq    %rax, %rdi
    call    __cxa_begin_catch
    # catch 块体
    movl    $-1, %ebx            # result = -1
    # __cxa_end_catch
    call    __cxa_end_catch
    jmp     .Lreturn             # 跳回正常路径
    .cfi_endproc
```

对应的 `.gcc_except_table`（概念表示）：

```
LSDA for process:
  LPStart:        process 函数起始地址
  TTBase:         type table 偏移
  Call Site Table:
    CS_start: compute(int) 调用位置
    CS_len:   5 (call 指令长度)
    CS_lp:    .Lexception0 偏移
    CS_action: 1 (→ Action Table 索引 1)
  Action Table:
    Action 1: filter=1 → type table [1]
              next=0
  Type Table:
    [1] = &typeid(std::exception)   # catch(const std::exception&)
```

### 13.3 `noexcept` 的效果对比

```cpp
int safe_compute(int x) noexcept {
    return x * 2;  // 不生成异常表
}

int unsafe_compute(int x) {
    return x * 2;  // 生成异常表（虽然实际不会抛）
}
```

```bash
# 对比两者的 .gcc_except_table
g++ -O2 -c example.cpp
readelf -x .gcc_except_table example.o
```

```
safe_compute:   .gcc_except_table 中没有 LSDA 条目
unsafe_compute: .gcc_except_table 中有 LSDA 条目
               （即使函数体很简单，也需要为外部调用生成 call site entry）
```

### 13.4 使用 `readelf` 阅读 `.eh_frame`

```bash
$ readelf --debug-dump=frames example.o

Contents of the .eh_frame section:

00000018 0000001c 0000001c FDE cie=00000000 pc=00000000..0000002a
   LOC           CFA      rbx   rbp   ra
0000000000000000 rsp+8    u     u     c-8
0000000000000002 rsp+16   c-8   u     c-8
0000000000000005 rsp+24   c-8   c-16  c-8
```

解读：
- `CFA = rsp+8`：函数入口时 CFA（调用者栈指针）= RSP + 8（跳过返回地址）
- `c-8`：该寄存器保存在 [CFA - 8] 处
- 每行对应一个指令地址的寄存器保存规则变化

---

## 十四、MSVC SEH 与 Itanium ABI 的差异

MSVC 使用 Windows SEH（Structured Exception Handling）实现 C++ 异常：

```
┌──────────────────────────────────────────────────────────────┐
│                 Itanium ABI          MSVC (x64)               │
│ 异常模型          table-driven       table-driven (x64)       │
│                              或      frame-based (x86/32)     │
│ 元数据存储        .eh_frame          .pdata + .xdata          │
│                   .gcc_except_table  (与 SEH 共享)            │
│ 展开信息格式      DWARF CFI          RUNTIME_FUNCTION         │
│ Personality       __gxx_personality  __CxxFrameHandler3/4     │
│ 异常对象          堆分配 (__cxa_*)   栈展开 + 编译器管理      │
│ SEH 硬件支持      N/A                Windows 内核级支持       │
│                                      (Vectored Exception)    │
│ 类型匹配          __dynamic_cast     编译器生成的 catch info  │
│ 异常链            __cxa_exception    ExceptionRegistration    │
└──────────────────────────────────────────────────────────────┘
```

关键差异：MSVC 在 x64 上使用 `__CxxFrameHandler3` 作为 personality routine，异常表存储在 PE 文件的 `.pdata` 和 `.xdata` 段中，而不是 ELF 的 `.eh_frame`。在 x86 (32-bit) 上，MSVC 使用基于帧的 SEH（在栈上放置 `EXCEPTION_REGISTRATION` 结构），这会给正常路径带来开销。

---

## 十五、常见陷阱与最佳实践

### 15.1 析构函数中的异常

```cpp
// ❌ 致命：析构函数抛异常
struct Bad {
    ~Bad() {
        close(fd);  // 如果 close 失败并抛出异常
        // → 在栈展开期间抛出异常 → std::terminate()
    }
};

// ✅ 正确：析构函数吞掉异常
struct Good {
    ~Good() noexcept {
        try { close(fd); }
        catch (...) { /* 记录日志，不传播 */ }
    }
};
// C++11 起析构函数默认 noexcept，无需显式标注
```

### 15.2 异常与 `std::move`

```cpp
// ❌ 危险：移动操作完成后抛异常
void process(Widget w) {
    Widget other = std::move(w);  // 移动
    risky_call();                  // 如果抛异常
    // w 已经被移动，处于有效但未指定状态
    // 如果 process 是 copy assignment operator 的一部分
    // → 可能违反 basic guarantee
}

// ✅ 先做所有可能抛的操作，最后 noexcept commit
void safe_process(Widget w) {
    risky_call();                  // 可能抛，但 w 还没被移动
    Widget other = std::move(w);  // noexcept
}
```

### 15.3 `catch(...)` 的正确使用

```cpp
void worker() {
    try {
        do_work();
    } catch (...) {
        // 记录异常信息
        log_exception();
        // 方式 1: 重新抛出
        throw;
        // 方式 2: 转换为错误码
        // return ErrorCode::UNKNOWN;
        // 方式 3: 使用 exception_ptr 传递到其他线程
        // ep = std::current_exception();
    }
}
```

---

## 延伸阅读

- [Itanium C++ ABI Exception Handling](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html) — 权威规范
- [DWARF Standard](https://dwarfstd.org/) — `.eh_frame` 使用的 CFI 格式规范
- [libc++abi 源码](https://github.com/llvm/llvm-project/tree/main/libcxxabi/src) — `__cxa_throw` 等运行时函数的实现
- [libstdc++ 异常处理](https://github.com/gcc-mirror/gcc/blob/master/libstdc++-v3/libsupc++/eh_throw.cc) — GCC 的异常运行时
- [RAII 与资源管理](/topics/raii) — 异常安全的基石
- [编译器优化](/topics/compiler-optimizations) — 异常对优化管线的影响
- [性能优化](/topics/performance) — 异常成本的实测数据
