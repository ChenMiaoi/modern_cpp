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

待补：补上 spdlog 相对 iostream / `std::print` / `std::format` 的语义边界，以及同步/异步日志 API 的行为约定。

## 对象布局

待补：补 logger、sink 列表、formatter 持有方式与异步队列消息对象的结构关系图。

## 核心源码路径

本文开头已给出 `include/spdlog/`；后续补 `logger.h`、`async_logger.h`、`details/thread_pool.h`、常见 sink 头文件的入口链。

## 核心类 / 函数

待补：统一整理 `logger`、`sink`、`formatter`、`async_logger`、`thread_pool`、`log_msg`、`log_msg_buffer`。

## 关键算法

待补：补充级别过滤、格式化分发、sink 广播、异步入队/出队与丢弃策略的关键路径。

## ABI 约束

待补：说明 spdlog 主要依赖头文件模板与内联实现，兼容性更多受 API 与配置宏影响，而不是稳定 ABI。

## 异常安全

待补：补充 sink 写出失败、格式化失败、异步队列满以及后台线程异常时的传播策略。

## iterator / reference invalidation

待补：本文主题不是容器 iterator；后续这里补 logger 内部 sink 列表变更、格式化缓冲区复用与异步消息对象生命周期的有效期规则。

## 性能模型

正文已经给出编译期级别裁剪与异步模式；后续补格式化成本、sink fan-out、队列争用与后台线程批量写出的性能模型。

## libstdc++ vs libc++ vs MSVC

待补：这里主要与标准库输出设施对照，并说明 spdlog 在不同标准库上的 fmt/线程实现差异与兼容点。

## 最小复现代码

```cpp
#include <spdlog/spdlog.h>

int main() {
  spdlog::info("value = {}", 42);
}
```

## 编译 / 反汇编 / benchmark 证据

待补：补上同步/异步 logger 热路径、级别裁剪后的代码生成，以及不同 sink 组合的 benchmark 证据。

## cpplings 练习入口

- [`format1` — std::format 格式化](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println 格式化输出](../../../exercises/cpp23/print23.cpp)
- [`jthread1` — std::jthread 与 stop_token](../../../exercises/cpp20/jthread1.cpp)
