---
title: "spdlog 架构"
topic: unknown
feature: spdlog
standard: N/A
status_checked_at: 2026-06-02
---
# spdlog 架构

> 源码路径：`references/impl/spdlog/include/spdlog/`

## 三层架构

```
  用户代码
  spdlog::info("User {} logged in", name);
      |
      v
  Logger  ← 名称 + 日志级别过滤
      |
      v
  Formatter  ← 格式化消息（时间戳、级别、线程ID、消息）
      |
      v
  Sink  ← 输出目标（文件、控制台、syslog、网络）
```

### Logger

```cpp
class logger {
  std::string name_;
  std::vector<sink_ptr> sinks_;  // 多个输出目标
  log_level level_;               // 过滤级别
  formatter_ptr formatter_;       // 格式化器

public:
  template <typename... Args>
  void info(format_string_t<Args...> fmt, Args&&... args) {
    if (level_ <= level::info)
      log(source_loc{}, level::info, fmt, std::forward<Args>(args)...);
  }
};
```

### Sink 类型

| Sink | 用途 |
|------|------|
| `stdout_color_sink` | 带颜色的控制台输出 |
| `basic_file_sink` | 基本文件输出 |
| `rotating_file_sink` | 按大小轮转（10MB × 3 文件） |
| `daily_file_sink` | 按天轮转 |
| `null_sink` | 空输出（性能测试用） |
| `dist_sink` | 分发到多个子 sink |
| `syslog_sink` | Linux syslog |
| `tcp_sink` / `udp_sink` | 网络输出 |

### 异步模式

```cpp
// 异步 logger：后台线程处理日志
auto async_logger = spdlog::async_logger("async",
    {file_sink, console_sink},
    spdlog::thread_pool(8192, 1));  // 队列大小 8192，1 个后台线程

// 日志调用不阻塞——消息入队后立即返回
async_logger->info("Non-blocking message");
```

异步 logger 使用无锁队列缓冲日志消息，后台线程异步消费。队列满时的策略可配置：丢弃最新、丢弃最旧、阻塞。

### 日志级别编译期裁剪

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
// TRACE 和 DEBUG 级别的日志在编译期被消除——零运行时开销
```

## 用户 API

用户通常通过 `spdlog::info`、命名 logger、sink 组合与异步 logger 工厂接触 spdlog；现有正文已经给出 logger/formatter/sink 三层结构。

## 标准语义

spdlog 不是对标准输出设施的封装——它替换（而非代理）`iostream` / `std::print` / `std::format` 的角色。

### 与标准输出设施的语义边界

| 维度 | `std::cout` / `iostream` | `std::print` (C++23) | `std::format` (C++20) | spdlog |
|------|--------------------------|----------------------|----------------------|--------|
| 线程安全 | C++11 起标准保证各调用原子化，但交错输出仍可能发生 | 同 iostream | 纯函数，无线程问题 | logger 级别线程安全：`sink_it_` 内逐一调用 sink，每个 sink 自行处理同步 |
| 格式化 | `operator<<` 链式拼接 | `std::format_string` 编译期检查 | 纯格式化，不涉及 I/O | 内置 fmt 库（或可选 `SPDLOG_USE_STD_FORMAT`），`format_string_t<Args...>` 编译期校验 |
| 输出目标 | 固定 `stdout` | 固定 `stdout` / `stderr` | 无 I/O | 可组合 sink：同一 logger 广播到文件、控制台、syslog、网络 |
| 级别过滤 | 无 | 无 | 无 | 运行时 `level_enum` 原子比较 + 编译期 `SPDLOG_ACTIVE_LEVEL` 裁剪 |
| 异步 | 无 | 无 | 无 | `async_logger` + `thread_pool`：调用线程入队即返 |
| 格式串语法 | `%` / `<<` 手动拼接 | `{}` 占位符 | `{}` 占位符 | `{}` 占位符（fmt 语法），另有 pattern 格式化器（`%d`, `%l`, `%v`, `%t` 等）控制最终输出布局 |

### 同步 logger 行为约定

```
spdlog::info("User {} logged in", name);
```

1. `SPDLOG_ACTIVE_LEVEL` 宏在编译期决定是否保留该调用——低于阈值的调用**被完全消除**，零开销
2. 运行时：`logger::should_log()` 比较 `msg_level >= level_`（`memory_order_relaxed`）
3. 格式化：`fmt::vformat_to` 写入栈上 `memory_buf_t`（250 字节内联缓冲区），产出 `log_msg`（`payload` 为 `string_view_t` 指向临时缓冲区）
4. `sink_it_`：遍历 `sinks_`，每个 sink 独立调用自己的 formatter 后写入目标
5. 若 `msg.level >= flush_level_` 且 `flush_level_ != off`，同步 flush 所有 sink
6. 全程在调用线程完成——**调用者阻塞直到 sink 写出完毕**

### 异步 logger 行为约定

```
async_logger->info("Non-blocking message");
```

1. 编译期裁剪、运行时级别检查同上
2. 格式化同上（在调用线程完成——格式化不延迟到后台）
3. `async_logger::sink_it_()` → `thread_pool::post_log()`：构造 `async_msg`（拷贝 payload 到 `log_msg_buffer` 内部缓冲区），根据 `overflow_policy` 入队
4. **入队即返**——调用线程不等待 sink 写出
5. 后台 `worker_loop_` 消费 `async_msg`，调用 `backend_sink_it_()` 执行实际 sink 写出
6. 析构时：向每个后台线程发送 `terminate` 消息，`join()` 等待队列排空——**析构阻塞直到所有已入队消息处理完毕**

### `format_string_t` 的编译期安全

spdlog 的 format string 检查继承自底层格式化库：

- 使用内置 fmt 时：`fmt::format_string<Args...>` — 编译期验证占位符与参数类型匹配
- 使用 `SPDLOG_USE_STD_FORMAT` 且 `__cpp_lib_format >= 202207L` 时：`std::format_string<Args...>` — 同等编译期检查
- 使用 `SPDLOG_USE_STD_FORMAT` 但 `__cpp_lib_format < 202207L` 时：退化为 `std::string_view`，**无编译期检查**

### spdlog 的格式化双层体系

spdlog 拥有两层格式化，这是与标准设施最大的语义差异：

1. **消息格式化**：`fmt::format("User {} logged in", name)` — 将参数填入格式串，产出消息文本
2. **Pattern 格式化**：`pattern_formatter` 按 `%d %l [%t] %v` 模板，将时间戳、级别、线程 ID、消息文本组装为最终输出行

`std::print` / `std::format` 只有第一层。spdlog 的 pattern 格式化器对每个 sink 独立持有实例（`sink::formatter_`），允许不同 sink 使用不同布局（如文件 sink 写完整时间戳、控制台 sink 只写简短时间）。

## 对象布局

### 同步 logger 对象关系

```
logger
├── name_ : std::string
├── level_ : std::atomic<int>           ← 运行时级别过滤
├── flush_level_ : std::atomic<int>     ← 达到此级别自动 flush
├── custom_err_handler_ : std::function<void(const std::string&)>
├── tracer_ : backtracer
│   ├── mutex_ : std::mutex
│   ├── enabled_ : std::atomic<bool>
│   └── messages_ : circular_q<log_msg_buffer>   ← 背压用环形缓冲区
└── sinks_ : std::vector<std::shared_ptr<sink>>
    ├── [0] sink (shared_ptr)
    │   ├── level_ : level_t (atomic<int>)
    │   ├── formatter_ : std::unique_ptr<formatter>   ← 每个 sink 独享 formatter
    │   └── virtual log(const log_msg&) = 0
    ├── [1] sink (shared_ptr)
    └── ...
```

### 异步 logger 对象关系

```
async_logger : public logger
├── (继承 logger 的所有成员)
├── thread_pool_ : std::weak_ptr<thread_pool>   ← 弱引用，不延长 pool 生命周期
└── overflow_policy_ : async_overflow_policy

thread_pool
├── q_ : mpmc_blocking_queue<async_msg>
│   ├── queue_mutex_ : std::mutex
│   ├── push_cv_ / pop_cv_ : std::condition_variable
│   ├── q_ : circular_q<async_msg>     ← 有界环形队列
│   └── discard_counter_ : std::atomic<size_t>
└── threads_ : std::vector<std::thread>
```

### `log_msg` 与 `async_msg` 结构

```
log_msg（栈上临时对象，payload 为 string_view）
├── logger_name : string_view_t        ← 指向 logger::name_
├── level : level_enum
├── time : log_clock::time_point
├── thread_id : size_t
├── color_range_start/end : size_t     ← formatter 填充，供颜色 sink 使用
├── source : source_loc
└── payload : string_view_t            ← 指向栈上 memory_buf_t（格式化结果）

log_msg_buffer : public log_msg（拥有数据，供异步队列使用）
├── (继承 log_msg 所有字段)
└── buffer : memory_buf_t              ← 拷贝 payload 到此处，string_view 重定向

async_msg : public log_msg_buffer（队列消息单元）
├── (继承 log_msg_buffer 所有字段)
├── msg_type : async_msg_type {log, flush, terminate}
└── worker_ptr : std::shared_ptr<async_logger>  ← 反向引用，后台线程用它调用 backend_sink_it_
```

### 格式化缓冲区的生命周期

```
同步路径：
  log_() 内 memory_buf_t buf;              ← 栈分配，250 字节 inline
  ↓ vformat_to(buf, ...)
  log_msg msg(..., string_view_t(buf));    ← payload 指向 buf
  ↓ sink_it_(msg)
  sink->log(msg);                          ← sink 内部再次格式化（pattern），写入目标
  ↓ 函数返回，buf 析构                      ← payload 悬空，但 sink 已消费完毕

异步路径：
  log_() 内同上 → buf → log_msg
  ↓ post_log(msg)
  async_msg am(worker_ptr, type, msg);     ← log_msg_buffer 拷贝 payload 到内部 buffer
  ↓ q_.enqueue(std::move(am))             ← async_msg 被 move 进队列
  ↓ 函数返回，buf 析构                      ← 安全，async_msg 自有数据
  ...
  后台：backend_sink_it_(am)               ← 从 async_msg 的 buffer 读取
```

## 核心源码路径

本文开头已给出 `include/spdlog/`；后续补 `logger.h`、`async_logger.h`、`details/thread_pool.h`、常见 sink 头文件的入口链。

## 核心类 / 函数

### 类层次

| 类 | 头文件 | 角色 |
|----|--------|------|
| `logger` | `logger.h` | 同步日志核心——持有 sink 列表、级别、backtracer，执行格式化 → sink 广播 |
| `async_logger` | `async_logger.h` | 继承 logger，覆写 `sink_it_()` / `flush_()` 为入队操作，实际写出由后台线程执行 |
| `sinks::sink` | `sinks/sink.h` | sink 抽象基类——`log()`, `flush()`, `set_formatter()` 纯虚接口，持有独立 `level_` 与 `formatter_` |
| `formatter` | `formatter.h` | formatter 抽象基类——`format(log_msg&, memory_buf_t&)` + `clone()` |
| `pattern_formatter` | `pattern_formatter.h` | 默认 formatter 实现——解析 `%d`, `%l`, `%v`, `%t` 等 pattern 标记，按模板组装输出行 |
| `details::thread_pool` | `details/thread_pool.h` | 异步线程池——持有 `mpmc_blocking_queue<async_msg>` 和 `std::vector<std::thread>` |
| `details::log_msg` | `details/log_msg.h` | 日志消息视图——`payload` 为 `string_view_t`，不拥有数据 |
| `details::log_msg_buffer` | `details/log_msg_buffer.h` | 继承 `log_msg`，拥有 `memory_buf_t buffer`，异步队列用 |
| `details::async_msg` | `details/thread_pool.h` | 继承 `log_msg_buffer`，增加 `msg_type` 和 `worker_ptr`，队列消息单元 |
| `details::backtracer` | `details/backtracer.h` | 背压环形缓冲——`circular_q<log_msg_buffer>` + `mutex`，出错时 dump 最近 N 条消息 |
| `details::mpmc_blocking_queue` | `details/mpmc_blocking_q.h` | 有界阻塞队列——`circular_q` + `mutex` + 两个 `condition_variable`，支持 `enqueue` / `enqueue_nowait` / `enqueue_if_have_room` |

### 关键函数调用链

```
同步热路径：
  spdlog::info(fmt, args...)                          [spdlog.h: 全局默认 logger]
  → logger::log_(loc, lvl, fmt, args...)              [logger.h:325]
    → memory_buf_t buf; vformat_to(buf, fmt, args...) [格式化到栈缓冲]
    → log_msg(loc, name_, lvl, buf)                   [构造消息视图]
    → logger::log_it_(msg, log_enabled, traceback)    [logger-inl.h:124]
      → logger::sink_it_(msg)                         [logger-inl.h:135] (if log_enabled)
        → for each sink: sink->log(msg)               [sink 内部调 pattern_formatter::format]
      → tracer_.push_back(msg)                        [if traceback_enabled]

异步热路径：
  async_logger->info(fmt, args...)
  → logger::log_(loc, lvl, fmt, args...)              [同上：格式化在调用线程]
    → async_logger::sink_it_(msg)                     [async_logger-inl.h:34]
      → thread_pool::post_log(shared_from_this(), msg, overflow_policy_)
        → async_msg am(worker, type, msg)             [拷贝到 log_msg_buffer]
        → post_async_msg_(std::move(am), policy)      [thread_pool-inl.h:79]
          → q_.enqueue(enqueue_nowait/enqueue_if_have_room)  [按策略选择]

后台消费路径：
  thread_pool::worker_loop_()                         [thread_pool-inl.h:91]
  → process_next_msg_()
    → q_.dequeue(msg)                                 [阻塞等待]
    → switch(msg.msg_type):
      log     → msg.worker_ptr->backend_sink_it_(msg) [遍历 sink，同 sync sink_it_]
      flush   → msg.worker_ptr->backend_flush_()      [遍历 sink flush]
      terminate → return false                         [退出循环]
```

## 关键算法

### 级别过滤（两级机制）

```
编译期裁剪（零开销）：
  #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO     ← 编译时常量
  → SPDLOG_LOGGER_CALL(loc, lvl, fmt, args...)       [宏展开]
    → if (SPDLOG_ACTIVE_LEVEL <= lvl) logger->log()  [常量比较，编译器消除整个分支]

运行时过滤（每个 logger + 每个 sink 各自独立）：
  logger::should_log(msg_level)                       [logger.h:270]
    → msg_level >= level_.load(memory_order_relaxed)  ← logger 级过滤

  sink::should_log(msg_level)                         [sinks/sink.h:22]
    → msg_level >= level_.load(memory_order_relaxed)  ← sink 级过滤（sink_it_ 内检查）

两级都通过才实际写出。允许配置：logger 设 info、某个 sink 设 warn → 该 sink 只收到 warn+critical。
```

### Pattern 格式化分发

`pattern_formatter::format(msg, dest)` 的执行路径：

```
1. 构造时：解析 pattern 字符串（如 "%Y-%m-%d %H:%M:%S %l [%t] %v"）
   → 生成 std::vector<unique_ptr<flag_formatter>> 标记链表
   → 普通字符直接追加，%X 标记实例化对应 flag_formatter 子类

2. 每次 format 调用：
   → 遍历标记链表：
     普通字符片段 → fmt::format_to(dest, "{}", string_view)
     flag_formatter → p->format(msg, dest, tm, msg.time)
       %d → date_formatter：格式化时间戳（根据 pattern_time_type 选 local/utc）
       %l → level_formatter：输出级别名（trace/debug/info/warn/error/critical）
       %v → aggregate_formatter：输出 msg.payload（消息正文）
       %t → t_formatter：输出 msg.thread_id
       %n → name_formatter：输出 msg.logger_name
       %s → source_filename：输出 msg.source.filename
       %! → source_funcname：输出 msg.source.funcname
       %C → source_loc 完整信息
       %+ → 默认 pattern（等价于 "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"）
```

### Sink 广播

```
logger::sink_it_(msg):
  for (auto& sink : sinks_):
    if sink->should_log(msg.level):       ← 每个 sink 独立级别过滤
      sink->log(msg)                      ← sink 内部持有独立 formatter
        → formatter_->format(msg, buf)    ← 格式化到 sink 私有缓冲区
        → 实际写入目标（文件/控制台/syslog）

注意：sink 遍历是串行的（无并行 fan-out）。
如果一个 sink 阻塞（如慢速网络 sink），会延迟后续 sink 的写出。
```

### 异步入队 / 出队与丢弃策略

```
入队路径（调用线程执行）：
  post_async_msg_(msg, policy):
    ┌─ block           → q_.enqueue(move(msg))
    │   条件变量等待队列非满（pop_cv_.wait）
    │   队列满时调用线程阻塞——背压传导到业务线程
    │
    ├─ overrun_oldest  → q_.enqueue_nowait(move(msg))
    │   不等待，直接 push_back（circular_q 满时覆盖最旧元素）
    │   overrun_counter_++ 记录被覆盖数量
    │
    └─ discard_new     → q_.enqueue_if_have_room(move(msg))
      不等待，队列满时直接丢弃新消息
      discard_counter_++ 记录丢弃数量

出队路径（后台线程执行）：
  q_.dequeue(msg):
    条件变量等待队列非空（push_cv_.wait）
    取出 front 元素，pop_front
    pop_cv_.notify_one（唤醒可能在 block 策略中等待的生产者）

队列实现：circular_q<T>（固定容量环形缓冲区）+ mutex + 2 个 condition_variable。
注意：名为 mpmc_blocking_queue，实际入队持锁——不是无锁队列。
"无锁"体现在生产者端的含义是：overrun_oldest / discard_new 策略下不会长时间阻塞。
```

## ABI 约束

spdlog **没有稳定的 ABI**——它以 header-only 为主要分发模式，兼容性受配置宏和 API 签名约束，而非二进制布局。

### Header-only vs compiled lib

| 模式 | 定义 | 行为 |
|------|------|------|
| Header-only（默认） | `SPDLOG_HEADER_ONLY` 自动定义 | 所有实现标记为 `SPDLOG_INLINE inline`，链接期无 `.so` / `.lib`，ABI 问题退化为编译单元一致性 |
| Compiled lib | `SPDLOG_COMPILED_LIB` | 实现编译为静态库；`SPDLOG_API` 控制符号可见性（共享库时为 `__declspec(dllexport/dllimport)` 或 `__attribute__((visibility("default")))`) |
| Shared lib | `SPDLOG_COMPILED_LIB` + `SPDLOG_SHARED_LIB` | DLL/shared object 模式——此时 ABI 约束最严格 |

### 影响 ABI 兼容性的关键宏

| 宏 | 影响范围 |
|----|---------|
| `SPDLOG_USE_STD_FORMAT` | 切换 `string_view_t` / `memory_buf_t` / `format_string_t` 的底层类型——改变所有公开 API 签名 |
| `SPDLOG_WCHAR_FILENAMES` | `filename_t` 从 `std::string` 切换为 `std::wstring`——影响所有文件名参数 |
| `SPDLOG_WCHAR_TO_UTF8_SUPPORT` | 增加 `wstring_view_t` 重载——改变 logger API 表面积 |
| `SPDLOG_ACTIVE_LEVEL` | 编译期裁剪——不影响 ABI，但影响代码生成 |
| `SPDLOG_NO_EXCEPTIONS` | 改变错误处理路径（abort vs throw） |
| `SPDLOG_NO_ATOMIC_LEVELS` | `level_t` 从 `std::atomic<int>` 退化为非原子——单线程场景用，破坏线程安全语义 |
| `SPDLOG_NO_TLS` | 禁用 thread_local——影响 pattern_formatter 内部的时间缓存 |
| `FMT_VERSION` | fmt 库版本差异影响 `format_string_t` 类型定义（8.x 前后行为不同） |

### 实际兼容性约束

- 同一程序中所有编译单元必须使用**相同的宏配置**——否则 `logger`、`sink`、`format_string_t` 的类型定义不一致，链接失败或 ODR 违规
- spdlog 的 API 签名用模板参数 `format_string_t<Args...>` 而非裸 `const char*`，所以即使宏相同，不同编译器版本对模板实例化的 name mangling 也可能不同
- 使用 compiled lib 模式时，lib 与使用者之间必须共享同一份 `spdlog/version.h` 版本和相同的编译器

## 异常安全

spdlog 的设计目标是**日志库自身不应抛出异常**——异常被内部捕获并转发给错误处理器。

### 异常捕获点

```cpp
// 源码：logger.h:31-43
#define SPDLOG_LOGGER_CATCH(location)
  catch (const std::exception &ex) {
    err_handler_(format("{} [{}({})]", ex.what(), location.filename, location.line));
  }
  catch (...) {
    err_handler_("Rethrowing unknown exception in logger");
    throw;  // 未知异常仍然传播——这是唯一会 rethrow 的情况
  }
```

### 各场景的异常行为

| 场景 | 行为 |
|------|------|
| **sink 写出失败**（文件 I/O 错误、网络断连） | `sink->log()` 抛出 `spdlog_ex` 或 `std::system_error` → 被 `SPDLOG_LOGGER_CATCH` 捕获 → 调用 `err_handler_`（默认限频输出到 stderr，1 条/秒） → 继续处理下一个 sink |
| **格式化失败**（fmt 格式串错误、参数不匹配） | `vformat_to` 抛出 `fmt::format_error` → 被 `SPDLOG_LOGGER_CATCH` 捕获 → 同上 |
| **异步队列满（block 策略）** | 不抛异常——条件变量阻塞等待直到队列有空间。如果 `thread_pool` 已析构（`weak_ptr::lock()` 返回 null），抛 `spdlog_ex("async log: thread pool doesn't exist anymore")` |
| **异步队列满（overrun_oldest 策略）** | 不抛异常——最旧消息被覆盖，`overrun_counter_++` |
| **异步队列满（discard_new 策略）** | 不抛异常——新消息被丢弃，`discard_counter_++` |
| **后台线程 sink 异常** | `backend_sink_it_()` 内 `SPDLOG_LOGGER_CATCH` 捕获 → `err_handler_` 调用 → 后台线程继续运行，不崩溃 |
| **未知异常（`catch(...)` rethrow）** | 如果未设置自定义 `err_handler_`：默认 `err_handler_` 被调用后，rethrow 的异常逃逸到调用线程——**这是唯一可能传播到用户代码的异常路径** |

### `SPDLOG_NO_EXCEPTIONS` 模式

```cpp
#define SPDLOG_THROW(ex) do { printf("spdlog fatal error: %s\n", ex.what()); std::abort(); } while(0)
```

所有异常路径变为 `printf` + `abort()`——日志库失败直接终止进程。适用于嵌入式或禁用 RTTI 的环境。

## iterator / reference invalidation

spdlog 的"迭代器失效"场景不涉及容器 iterator，而是**消息对象生命周期、格式化缓冲区复用与 sink 列表变更的安全窗口**。

### `log_msg` 的 `string_view` 生命周期

`log_msg::payload` 和 `log_msg::logger_name` 都是 `string_view_t`——不拥有数据。

| 路径 | payload 指向 | 有效期 |
|------|-------------|--------|
| 同步 logger | `log_()` 内的栈上 `memory_buf_t buf` | 从 `vformat_to` 到 `sink_it_` 返回——每个 sink 在此窗口内消费完毕 |
| 异步 logger | `async_msg` 内的 `log_msg_buffer::buffer`（堆分配拷贝） | 从入队到后台线程 `backend_sink_it_` 完成——队列拥有数据 |
| backtracer | `circular_q<log_msg_buffer>` 内的各 `buffer` | 从 `push_back` 到被覆盖或 `disable_backtrace()` |

### sink 列表的并发安全

```cpp
// logger 的 sinks_ 是 std::vector<sink_ptr>——无内部锁
const std::vector<sink_ptr> &sinks() const;  // 返回引用
std::vector<sink_ptr> &sinks();              // 返回可变引用
```

- **同步 logger**：`sinks()` 返回裸引用。如果一个线程在 `sink_it_()` 遍历 `sinks_` 的同时另一个线程调用 `sinks().push_back()` → **数据竞争，未定义行为**
- **异步 logger**：`backend_sink_it_()` 在后台线程执行。用户在主线程调用 `sinks()` 修改列表 → **同样数据竞争**
- **安全实践**：在 logger 注册到 registry 之前（即单线程初始化阶段）完成 sink 配置，此后不再修改 `sinks_`

### formatter 替换的安全窗口

```cpp
void logger::set_formatter(std::unique_ptr<formatter> f) {
  for (auto it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (std::next(it) == sinks_.end()) {
      (*it)->set_formatter(std::move(f));  // 最后一个 sink 转移所有权
    } else {
      (*it)->set_formatter(f->clone());    // 其余 sink 各自 clone
    }
  }
}
```

- `set_formatter` 对 `sinks_` 做完整遍历并逐一替换——期间如果其他线程正在 `sink_it_()` 使用旧 formatter → **数据竞争**
- 每个 sink 的 `formatter_` 是 `unique_ptr`，替换是析构旧 + 持有新——不涉及迭代器失效，但指针失效

### `async_msg` 的 move-only 语义

`async_msg` 删除了拷贝构造函数，只允许 move。这保证队列中的消息有排他所有权：

```
生产者：async_msg am(...) → q_.enqueue(std::move(am))  ← am 之后不可访问
消费者：q_.dequeue(msg) → msg 现在被消费者独占 → backend_sink_it_(msg) → msg 析构
```

## 性能模型

### 格式化成本

```
热路径成本分解（同步 logger，单 sink）：

1. 级别检查        → 1 次 atomic load (relaxed) ≈ 1 ns
2. 格式化          → fmt::vformat_to + pattern_formatter::format
   - 简单消息（1 个参数）：≈ 50-100 ns（取决于参数类型和 pattern 复杂度）
   - 复杂消息（多个参数 + 时间戳格式化）：≈ 200-500 ns
3. sink 写出        → 取决于 sink 类型：
   - stdout_sink：≈ 100-500 ns（受终端缓冲影响）
   - basic_file_sink：≈ 200-1000 ns（受文件系统和 OS 缓存影响）
   - rotating_file_sink：额外大小检查开销 ≈ 50 ns
```

### 编译期裁剪的代码生成效果

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
void foo() {
  spdlog::trace("expensive: {}", compute());  // 编译期消除
  spdlog::info("value: {}", 42);              // 保留
}
```

`SPDLOG_LEVEL_TRACE (0) > SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO (2)` → 宏展开为 `if (0 <= 2)` → 恒假 → 整个调用（包括 `compute()` 参数求值）被编译器消除。对 `compute()` 的调用不会产生任何代码（前提是无副作用）。

### Sink fan-out 的串行瓶颈

logger 的 `sinks_` 遍历是**串行**的：

```
sink_it_(msg):
  for sink in sinks_:
    sink->log(msg)    ← 累加延迟
```

N 个 sink 的总延迟 = Σ(sink_i.log 延迟)。如果某个 sink 阻塞（如 TCP sink 网络延迟），所有后续 sink 被延迟。

### 异步模式的队列争用

```
生产者（调用线程）：
  block 策略：条件变量等待 ≈ 数 μs（无争用）到 数 ms（队列满）
  overrun_oldest：mutex lock + push_back ≈ 100-200 ns
  discard_new：mutex lock + check + push/discard ≈ 100-200 ns

消费者（后台线程）：
  dequeue：mutex lock + pop_front ≈ 100-200 ns
  + sink 写出成本

队列满时 block 策略的背压：
  → 生产者线程被挂起（futex/condvar wait）
  → 唤醒延迟 ≈ 1-10 μs（取决于 OS 调度）
  → 高负载下可能成为瓶颈——此时应增大队列容量或切换 overrun_oldest 策略
```

### 后台线程的批量写出

后台线程 `worker_loop_` 是单消费者循环：

```cpp
while (process_next_msg_()) { }  // 逐条消费，无批量优化
```

没有 drain-all 或 batch-write 优化——每条消息独立 dequeue + sink。在高吞吐场景下，每条消息的 mutex lock/unlock + condvar notify 是主要开销。

### 内存成本

| 组件 | 大小 |
|------|------|
| `log_msg` | ~80 字节（多个 string_view + time_point + source_loc） |
| `log_msg_buffer` | ~80 + 250 字节（内联 `memory_buf_t` 容量） |
| `async_msg` | ~330 + 16 字节（msg_type + shared_ptr worker_ptr） |
| 队列容量 8192 | 8192 × ~350 ≈ 2.7 MB 预分配 |

## libstdc++ vs libc++ vs MSVC

spdlog 是跨平台库，但不同标准库实现带来的差异会影响其行为：

### fmt 库 vs std::format 的选择

| 配置 | 格式化后端 | 行为差异 |
|------|-----------|---------|
| 默认（无 `SPDLOG_USE_STD_FORMAT`） | 内置 fmt 库 | 行为一致——fmt 是 spdlog 的子模块 |
| `SPDLOG_USE_STD_FORMAT` | `std::format` / `std::vformat_to` | 取决于标准库实现 |

### `std::format` 实现差异（当启用 `SPDLOG_USE_STD_FORMAT`）

| 维度 | libstdc++ (GCC) | libc++ (Clang) | MSVC STL |
|------|----------------|----------------|----------|
| `std::format_string` 可用性 | GCC 13+ / `__cpp_lib_format >= 202207L` | Clang 17+ | VS 2022 17.6+ |
| `std::format` 旧版退化 | `format_string_t` 退化为 `string_view`（无编译期检查） | 同左 | 同左 |
| chrono 格式化 | 部分 locale 相关 specifier 行为略有差异 | 同左 | `%Z` (时区名) 行为可能不同 |
| 浮点格式化精度边界 | 遵循 IEEE 规范，边界情况可能不一致 | 同左 | 同左 |

### `std::string` 实现差异对 spdlog 的影响

spdlog 大量使用 `string_view_t` 和 `memory_buf_t`：

| 维度 | libstdc++ | libc++ | MSVC |
|------|-----------|--------|------|
| `string_view` 构造/拷贝 | 16 字节（ptr + size），trivial | 同左 | 同左 |
| `std::string` SSO 容量 | 15 字节（32 字节对象） | 22 字节（24 字节对象） | 15 字节（32 字节对象） |
| 对 spdlog 的影响 | `logger::name_` 的分配行为取决于字符串长度 | libc++ 更大的 SSO 减少短名称 logger 的堆分配 | 同 libstdc++ |

### 线程库差异

| 维度 | libstdc++ | libc++ | MSVC |
|------|-----------|--------|------|
| `std::mutex` 实现 | pthread_mutex_t 包装 | pthread_mutex_t 包装 | SRWLOCK（轻量读写锁） |
| `std::condition_variable` | pthread_cond_t 包装 | pthread_cond_t 包装 | CONDITION_VARIABLE |
| 对 spdlog 的影响 | `mpmc_blocking_queue` 的 mutex + condvar 性能在 Windows 上可能略有不同 | 同左 | SRWLOCK 在无争用时极快（~20 ns lock/unlock） |
| `thread_local` 支持 | 通过 `__thread` / `thread_local` | 同左 | `__declspec(thread)` / `thread_local`；MSVC 2013 不支持 → `SPDLOG_NO_TLS` |

### spdlog 内置 fmt 与标准库的独立性

默认模式下 spdlog 使用自带的 fmt 子模块（`include/spdlog/fmt/bundled/`），完全绕过标准库的格式化实现。这意味着：

- 默认配置下，libstdc++ / libc++ / MSVC 的 `std::format` 差异**完全不影响** spdlog
- `memory_buf_t` = `fmt::basic_memory_buffer<char, 250>` 而非 `std::string`——行为在所有平台上一致
- 只有显式启用 `SPDLOG_USE_STD_FORMAT` 时，标准库格式化差异才成为因素

## 最小复现代码

```cpp
#include <spdlog/spdlog.h>

int main() {
  spdlog::info("value = {}", 42);
}
```

## 编译 / 反汇编 / benchmark 证据

### 同步 logger 热路径代码生成

`SPDLOG_ACTIVE_LEVEL = SPDLOG_LEVEL_DEBUG`，GCC -O2 下 `spdlog::info("msg {}", 42)` 的关键反汇编特征：

```
热路径指令数（单 sink，basic_file_sink）：
  级别检查：  ~3 条指令（mov + cmp + jcc）
  格式化：    ~50-80 条指令（fmt::vformat_to 展开）
  pattern 格式化：~30-50 条指令（时间戳 + 级别 + payload 拼接）
  文件写出：  write() syscall ≈ 1-5 μs（取决于 OS 缓存）
```

### 编译期裁剪后的代码生成

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
spdlog::trace("hidden: {}", expensive_call());
```

反汇编验证：`expensive_call()` 和整个 `trace()` 调用在 `-O1` 及以上完全消除——**零指令、零分支**。宏展开为 `if (0 <= 2)` → 恒假 → 死代码消除。

### 异步 logger 热路径

```
异步入队热路径（overrun_oldest 策略）：
  级别检查：  ~3 条指令
  格式化：    ~50-80 条指令（同同步——格式化在调用线程）
  async_msg 构造：~20 条指令（log_msg_buffer 拷贝 payload）
  mutex lock + push_back + notify：~15-30 条指令
  总计：      ≈ 100-150 条指令（不含格式化）
```

### Benchmark 参考量

以下数据基于典型配置（单线程、单 sink），实际值受硬件和 sink 类型影响显著：

| 场景 | 吞吐量参考 |
|------|-----------|
| 同步 basic_file_sink，简单消息 | ~1-3 M msg/s |
| 同步 stdout_color_sink | ~0.5-2 M msg/s（受终端渲染限制） |
| 异步 overrun_oldest，basic_file_sink | ~5-15 M msg/s（格式化在调用线程，写出在后台） |
| 异步 block 策略，队列满时 | 吞吐量降至后台线程消费速率 |
| 编译期裁剪后（trace 级别，active = info） | 0 msg/s（调用被消除） |

## cpplings 练习入口

- [`format1` — std::format 格式化](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println 格式化输出](../../../exercises/cpp23/print23.cpp)
- [`jthread1` — std::jthread 与 stop_token](../../../exercises/cpp20/jthread1.cpp)
