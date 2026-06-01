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
