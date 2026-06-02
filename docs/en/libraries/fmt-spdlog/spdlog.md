---
title: "spdlog 架构"
topic: unknown
feature: spdlog
standard: N/A
status_checked_at: 2026-06-02
---
# spdlog Architecture

> Source path: `references/impl/spdlog/include/spdlog/`

## Three-Layer Architecture

```
  User Code
  spdlog::info("User {} logged in", name);
      |
      v
  Logger  <- name + log level filtering
      |
      v
  Formatter  <- format message (timestamp, level, thread ID, message)
      |
      v
  Sink  <- output target (file, console, syslog, network)
```

### Logger

```cpp
class logger {
  std::string name_;
  std::vector<sink_ptr> sinks_;  // Multiple output targets
  log_level level_;               // Filtering level
  formatter_ptr formatter_;       // Formatter

public:
  template <typename... Args>
  void info(format_string_t<Args...> fmt, Args&&... args) {
    if (level_ <= level::info)
      log(source_loc{}, level::info, fmt, std::forward<Args>(args)...);
  }
};
```

### Sink Types

| Sink | Purpose |
|------|---------|
| `stdout_color_sink` | Colored console output |
| `basic_file_sink` | Basic file output |
| `rotating_file_sink` | Size-based rotation (10MB × 3 files) |
| `daily_file_sink` | Daily rotation |
| `null_sink` | Null output (for performance testing) |
| `dist_sink` | Distributes to multiple sub-sinks |
| `syslog_sink` | Linux syslog |
| `tcp_sink` / `udp_sink` | Network output |

### Async Mode

```cpp
// Async logger: background thread processes logs
auto async_logger = spdlog::async_logger("async",
    {file_sink, console_sink},
    spdlog::thread_pool(8192, 1));  // Queue size 8192, 1 background thread

// Log call does not block — message is enqueued and returns immediately
async_logger->info("Non-blocking message");
```

Async logger uses a lock-free queue to buffer log messages, with a background thread consuming them asynchronously. The policy when the queue is full is configurable: discard newest, discard oldest, block.

### Compile-Time Log Level Pruning

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
// TRACE and DEBUG level logs are eliminated at compile time — zero runtime overhead
```

## User API

Users typically interact with spdlog through `spdlog::info`, named loggers, sink combinations, and async logger factories; the existing text already covers the logger/formatter/sink three-layer structure.

## Standard Semantics

spdlog is not a wrapper around standard output facilities — it replaces (rather than proxies) the role of `iostream` / `std::print` / `std::format`.

### Semantic Boundary with Standard Output Facilities

| Dimension | `std::cout` / `iostream` | `std::print` (C++23) | `std::format` (C++20) | spdlog |
|-----------|--------------------------|----------------------|----------------------|--------|
| Thread safety | Since C++11, standard guarantees atomic per-call, but interleaved output may still occur | Same as iostream | Pure function, no thread issues | Logger-level thread safety: `sink_it_` calls sinks one by one, each sink handles synchronization independently |
| Formatting | `operator<<` chain concatenation | `std::format_string` compile-time checking | Pure formatting, no I/O | Built-in fmt library (or optional `SPDLOG_USE_STD_FORMAT`), `format_string_t<Args...>` compile-time validation |
| Output target | Fixed `stdout` | Fixed `stdout` / `stderr` | No I/O | Composable sinks: same logger broadcasts to file, console, syslog, network |
| Level filtering | None | None | None | Runtime `level_enum` atomic comparison + compile-time `SPDLOG_ACTIVE_LEVEL` pruning |
| Async | None | None | None | `async_logger` + `thread_pool`: calling thread enqueues and returns |
| Format string syntax | `%` / `<<` manual concatenation | `{}` placeholder | `{}` placeholder | `{}` placeholder (fmt syntax), plus pattern formatter (`%d`, `%l`, `%v`, `%t`, etc.) controlling final output layout |

### Synchronous Logger Behavior Contract

```
spdlog::info("User {} logged in", name);
```

1. The `SPDLOG_ACTIVE_LEVEL` macro determines at compile time whether to retain the call — calls below the threshold are **completely eliminated**, with zero overhead.
2. Runtime: `logger::should_log()` compares `msg_level >= level_` (`memory_order_relaxed`).
3. Formatting: `fmt::vformat_to` writes to a stack-allocated `memory_buf_t` (250-byte inline buffer), producing `log_msg` (`payload` is a `string_view_t` pointing to the temporary buffer).
4. `sink_it_`: iterates over `sinks_`, each sink independently calls its own formatter and writes to the target.
5. If `msg.level >= flush_level_` and `flush_level_ != off`, synchronously flushes all sinks.
6. Everything completes in the calling thread — **the caller blocks until sink writing is finished**.

### Async Logger Behavior Contract

```
async_logger->info("Non-blocking message");
```

1. Compile-time pruning and runtime level checking are the same as above.
2. Formatting is the same as above (completed in the calling thread — formatting is not deferred to the background).
3. `async_logger::sink_it_()` → `thread_pool::post_log()`: constructs `async_msg` (copies payload to `log_msg_buffer` internal buffer), enqueues according to `overflow_policy`.
4. **Enqueue and return** — the calling thread does not wait for sink writing.
5. Background `worker_loop_` consumes `async_msg`, calls `backend_sink_it_()` to perform actual sink writing.
6. On destruction: sends a `terminate` message to each background thread, `join()` waits for the queue to drain — **destruction blocks until all enqueued messages have been processed**.

### `format_string_t` Compile-Time Safety

spdlog's format string checking inherits from the underlying formatting library:

- When using built-in fmt: `fmt::format_string<Args...>` — compile-time validation that placeholders match argument types.
- When using `SPDLOG_USE_STD_FORMAT` with `__cpp_lib_format >= 202207L`: `std::format_string<Args...>` — equivalent compile-time checking.
- When using `SPDLOG_USE_STD_FORMAT` but `__cpp_lib_format < 202207L`: degrades to `std::string_view`, **no compile-time checking**.

### spdlog's Two-Layer Formatting System

spdlog has two layers of formatting, which is the biggest semantic difference from standard facilities:

1. **Message formatting**: `fmt::format("User {} logged in", name)` — fills arguments into the format string, producing the message text.
2. **Pattern formatting**: `pattern_formatter` assembles timestamp, level, thread ID, and message text into the final output line according to a `%d %l [%t] %v` template.

`std::print` / `std::format` only has the first layer. spdlog's pattern formatter holds independent instances per sink (`sink::formatter_`), allowing different sinks to use different layouts (e.g. file sink writes full timestamps, console sink writes only short timestamps).

## Object Layout

### Synchronous Logger Object Relationships

```
logger
├── name_ : std::string
├── level_ : std::atomic<int>           <- runtime level filtering
├── flush_level_ : std::atomic<int>     <- auto-flush at this level
├── custom_err_handler_ : std::function<void(const std::string&)>
├── tracer_ : backtracer
│   ├── mutex_ : std::mutex
│   ├── enabled_ : std::atomic<bool>
│   └── messages_ : circular_q<log_msg_buffer>   <- ring buffer for backpressure
└── sinks_ : std::vector<std::shared_ptr<sink>>
    ├── [0] sink (shared_ptr)
    │   ├── level_ : level_t (atomic<int>)
    │   ├── formatter_ : std::unique_ptr<formatter>   <- each sink has its own formatter
    │   └── virtual log(const log_msg&) = 0
    ├── [1] sink (shared_ptr)
    └── ...
```

### Async Logger Object Relationships

```
async_logger : public logger
├── (inherits all logger members)
├── thread_pool_ : std::weak_ptr<thread_pool>   <- weak reference, does not extend pool lifetime
└── overflow_policy_ : async_overflow_policy

thread_pool
├── q_ : mpmc_blocking_queue<async_msg>
│   ├── queue_mutex_ : std::mutex
│   ├── push_cv_ / pop_cv_ : std::condition_variable
│   ├── q_ : circular_q<async_msg>     <- bounded ring queue
│   └── discard_counter_ : std::atomic<size_t>
└── threads_ : std::vector<std::thread>
```

### `log_msg` and `async_msg` Structures

```
log_msg (stack-allocated temporary, payload is string_view)
├── logger_name : string_view_t        <- points to logger::name_
├── level : level_enum
├── time : log_clock::time_point
├── thread_id : size_t
├── color_range_start/end : size_t     <- filled by formatter, used by color sinks
├── source : source_loc
└── payload : string_view_t            <- points to stack-allocated memory_buf_t (format result)

log_msg_buffer : public log_msg (owns data, used by async queue)
├── (inherits all log_msg fields)
└── buffer : memory_buf_t              <- payload is copied here, string_view is redirected

async_msg : public log_msg_buffer (queue message unit)
├── (inherits all log_msg_buffer fields)
├── msg_type : async_msg_type {log, flush, terminate}
└── worker_ptr : std::shared_ptr<async_logger>  <- back-reference, background thread uses it to call backend_sink_it_
```

### Format Buffer Lifetime

```
Synchronous path:
  log_() internal memory_buf_t buf;              <- stack allocated, 250 bytes inline
  ↓ vformat_to(buf, ...)
  log_msg msg(..., string_view_t(buf));    <- payload points to buf
  ↓ sink_it_(msg)
  sink->log(msg);                          <- sink internally reformats (pattern), writes to target
  ↓ function returns, buf destroyed        <- payload dangling, but sink already consumed

Async path:
  log_() same as above → buf → log_msg
  ↓ post_log(msg)
  async_msg am(worker_ptr, type, msg);     <- log_msg_buffer copies payload to internal buffer
  ↓ q_.enqueue(std::move(am))             <- async_msg is moved into queue
  ↓ function returns, buf destroyed        <- safe, async_msg owns its data
  ...
  Background: backend_sink_it_(am)               <- reads from async_msg's buffer
```

## Core Source Paths

`include/spdlog/` was given at the beginning of this article; the entry chains for `logger.h`, `async_logger.h`, `details/thread_pool.h`, and common sink header files will follow.

## Core Classes / Functions

### Class Hierarchy

| Class | Header | Role |
|-------|--------|------|
| `logger` | `logger.h` | Synchronous logging core — holds sink list, level, backtracer; performs formatting → sink broadcast |
| `async_logger` | `async_logger.h` | Inherits logger, overrides `sink_it_()` / `flush_()` as enqueue operations; actual writing is performed by background threads |
| `sinks::sink` | `sinks/sink.h` | Sink abstract base class — `log()`, `flush()`, `set_formatter()` pure virtual interfaces, holds independent `level_` and `formatter_` |
| `formatter` | `formatter.h` | Formatter abstract base class — `format(log_msg&, memory_buf_t&)` + `clone()` |
| `pattern_formatter` | `pattern_formatter.h` | Default formatter implementation — parses `%d`, `%l`, `%v`, `%t` pattern flags and assembles output line from template |
| `details::thread_pool` | `details/thread_pool.h` | Async thread pool — holds `mpmc_blocking_queue<async_msg>` and `std::vector<std::thread>` |
| `details::log_msg` | `details/log_msg.h` | Log message view — `payload` is `string_view_t`, does not own data |
| `details::log_msg_buffer` | `details/log_msg_buffer.h` | Inherits `log_msg`, owns `memory_buf_t buffer`, used by async queue |
| `details::async_msg` | `details/thread_pool.h` | Inherits `log_msg_buffer`, adds `msg_type` and `worker_ptr`; queue message unit |
| `details::backtracer` | `details/backtracer.h` | Backpressure ring buffer — `circular_q<log_msg_buffer>` + `mutex`, dumps last N messages on error |
| `details::mpmc_blocking_queue` | `details/mpmc_blocking_q.h` | Bounded blocking queue — `circular_q` + `mutex` + two `condition_variable`s, supports `enqueue` / `enqueue_nowait` / `enqueue_if_have_room` |

### Key Function Call Chains

```
Synchronous hot path:
  spdlog::info(fmt, args...)                          [spdlog.h: global default logger]
  → logger::log_(loc, lvl, fmt, args...)              [logger.h:325]
    → memory_buf_t buf; vformat_to(buf, fmt, args...) [format to stack buffer]
    → log_msg(loc, name_, lvl, buf)                   [construct message view]
    → logger::log_it_(msg, log_enabled, traceback)    [logger-inl.h:124]
      → logger::sink_it_(msg)                         [logger-inl.h:135] (if log_enabled)
        → for each sink: sink->log(msg)               [sink internally calls pattern_formatter::format]
      → tracer_.push_back(msg)                        [if traceback_enabled]

Async hot path:
  async_logger->info(fmt, args...)
  → logger::log_(loc, lvl, fmt, args...)              [same as above: formatting in calling thread]
    → async_logger::sink_it_(msg)                     [async_logger-inl.h:34]
      → thread_pool::post_log(shared_from_this(), msg, overflow_policy_)
        → async_msg am(worker, type, msg)             [copy to log_msg_buffer]
        → post_async_msg_(std::move(am), policy)      [thread_pool-inl.h:79]
          → q_.enqueue(enqueue_nowait/enqueue_if_have_room)  [select by policy]

Background consumer path:
  thread_pool::worker_loop_()                         [thread_pool-inl.h:91]
  → process_next_msg_()
    → q_.dequeue(msg)                                 [blocks waiting]
    → switch(msg.msg_type):
      log     → msg.worker_ptr->backend_sink_it_(msg) [iterate sinks, same as sync sink_it_]
      flush   → msg.worker_ptr->backend_flush_()      [iterate sinks and flush]
      terminate → return false                         [exit loop]
```

## Key Algorithms

### Level Filtering (Two-Tier Mechanism)

```
Compile-time pruning (zero overhead):
  #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO     <- compile-time constant
  → SPDLOG_LOGGER_CALL(loc, lvl, fmt, args...)       [macro expansion]
    → if (SPDLOG_ACTIVE_LEVEL <= lvl) logger->log()  [constant comparison, compiler eliminates entire branch]

Runtime filtering (independent per logger + per sink):
  logger::should_log(msg_level)                       [logger.h:270]
    → msg_level >= level_.load(memory_order_relaxed)  <- logger-level filtering

  sink::should_log(msg_level)                         [sinks/sink.h:22]
    → msg_level >= level_.load(memory_order_relaxed)  <- sink-level filtering (checked inside sink_it_)

Writing occurs only when both tiers pass. Allows configuration: logger set to info, a specific sink set to warn → that sink only receives warn+critical.
```

### Pattern Formatting Dispatch

`pattern_formatter::format(msg, dest)` execution path:

```
1. Construction: parse pattern string (e.g. "%Y-%m-%d %H:%M:%S %l [%t] %v")
   → generate std::vector<unique_ptr<flag_formatter>> flag chain
   → plain characters appended directly, %X flags instantiate corresponding flag_formatter subclass

2. Each format call:
   → iterate flag chain:
     plain text fragments → fmt::format_to(dest, "{}", string_view)
     flag_formatter → p->format(msg, dest, tm, msg.time)
       %d → date_formatter: format timestamp (choose local/utc based on pattern_time_type)
       %l → level_formatter: output level name (trace/debug/info/warn/error/critical)
       %v → aggregate_formatter: output msg.payload (message body)
       %t → t_formatter: output msg.thread_id
       %n → name_formatter: output msg.logger_name
       %s → source_filename: output msg.source.filename
       %! → source_funcname: output msg.source.funcname
       %C → source_loc full information
       %+ → default pattern (equivalent to "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v")
```

### Sink Broadcast

```
logger::sink_it_(msg):
  for (auto& sink : sinks_):
    if sink->should_log(msg.level):       <- each sink filters independently by level
      sink->log(msg)                      <- sink internally holds independent formatter
        → formatter_->format(msg, buf)    <- format to sink's private buffer
        → write to actual target (file/console/syslog)

Note: sink iteration is serial (no parallel fan-out).
If one sink blocks (e.g. slow network sink), it delays subsequent sinks' output.
```

### Async Enqueue / Dequeue and Discard Policies

```
Enqueue path (executed by calling thread):
  post_async_msg_(msg, policy):
    ┌─ block           → q_.enqueue(move(msg))
    │   condition variable waits for non-full queue (pop_cv_.wait)
    │   when queue is full, calling thread blocks — backpressure propagates to business thread
    │
    ├─ overrun_oldest  → q_.enqueue_nowait(move(msg))
    │   no wait, direct push_back (circular_q overwrites oldest element when full)
    │   overrun_counter_++ records number of overwritten messages
    │
    └─ discard_new     → q_.enqueue_if_have_room(move(msg))
      no wait, new message discarded when queue is full
      discard_counter_++ records number of discarded messages

Dequeue path (executed by background thread):
  q_.dequeue(msg):
    condition variable waits for non-empty queue (push_cv_.wait)
    take front element, pop_front
    pop_cv_.notify_one (wake up producer possibly waiting in block policy)

Queue implementation: circular_q<T> (fixed-capacity ring buffer) + mutex + 2 condition_variable.
Note: despite being named mpmc_blocking_queue, enqueue actually holds a lock — it is not a lock-free queue.
The "lock-free" aspect on the producer side means: under overrun_oldest / discard_new policies, it does not block for extended periods.
```

## ABI Constraints

spdlog **does not have a stable ABI** — it distributes primarily as header-only, with compatibility constrained by configuration macros and API signatures rather than binary layout.

### Header-Only vs Compiled Lib

| Mode | Definition | Behavior |
|------|-----------|----------|
| Header-only (default) | `SPDLOG_HEADER_ONLY` automatically defined | All implementations marked as `SPDLOG_INLINE inline`, no `.so` / `.lib` at link time; ABI issues reduce to translation unit consistency |
| Compiled lib | `SPDLOG_COMPILED_LIB` | Implementation compiled as static library; `SPDLOG_API` controls symbol visibility (`__declspec(dllexport/dllimport)` or `__attribute__((visibility("default")))` for shared libraries) |
| Shared lib | `SPDLOG_COMPILED_LIB` + `SPDLOG_SHARED_LIB` | DLL/shared object mode — strictest ABI constraints |

### Key Macros Affecting ABI Compatibility

| Macro | Impact Scope |
|-------|-------------|
| `SPDLOG_USE_STD_FORMAT` | Switches `string_view_t` / `memory_buf_t` / `format_string_t` underlying types — changes all public API signatures |
| `SPDLOG_WCHAR_FILENAMES` | `filename_t` switches from `std::string` to `std::wstring` — affects all filename parameters |
| `SPDLOG_WCHAR_TO_UTF8_SUPPORT` | Adds `wstring_view_t` overloads — changes logger API surface |
| `SPDLOG_ACTIVE_LEVEL` | Compile-time pruning — does not affect ABI, but affects code generation |
| `SPDLOG_NO_EXCEPTIONS` | Changes error handling path (abort vs throw) |
| `SPDLOG_NO_ATOMIC_LEVELS` | `level_t` degrades from `std::atomic<int>` to non-atomic — for single-threaded scenarios, breaks thread safety semantics |
| `SPDLOG_NO_TLS` | Disables thread_local — affects time caching in pattern_formatter |
| `FMT_VERSION` | fmt library version differences affect `format_string_t` type definition (behavior differs before/after 8.x) |

### Actual Compatibility Constraints

- All translation units in the same program must use **identical macro configuration** — otherwise `logger`, `sink`, `format_string_t` type definitions are inconsistent, causing link failures or ODR violations.
- spdlog's API signatures use template parameter `format_string_t<Args...>` rather than raw `const char*`, so even with identical macros, different compiler versions may produce different name mangling for template instantiations.
- When using compiled lib mode, the lib and its users must share the same `spdlog/version.h` version and the same compiler.

## Exception Safety

spdlog's design goal is that **the logging library itself should not throw exceptions** — exceptions are internally caught and forwarded to error handlers.

### Exception Catch Points

```cpp
// Source: logger.h:31-43
#define SPDLOG_LOGGER_CATCH(location)
  catch (const std::exception &ex) {
    err_handler_(format("{} [{}({})]", ex.what(), location.filename, location.line));
  }
  catch (...) {
    err_handler_("Rethrowing unknown exception in logger");
    throw;  // Unknown exceptions still propagate — this is the only case that rethrows
  }
```

### Exception Behavior by Scenario

| Scenario | Behavior |
|----------|----------|
| **Sink write failure** (file I/O error, network disconnection) | `sink->log()` throws `spdlog_ex` or `std::system_error` → caught by `SPDLOG_LOGGER_CATCH` → calls `err_handler_` (default rate-limited output to stderr, 1 msg/sec) → continues processing next sink |
| **Formatting failure** (fmt format string error, argument mismatch) | `vformat_to` throws `fmt::format_error` → caught by `SPDLOG_LOGGER_CATCH` → same as above |
| **Async queue full (block policy)** | No exception — condition variable blocks until queue has space. If `thread_pool` has been destroyed (`weak_ptr::lock()` returns null), throws `spdlog_ex("async log: thread pool doesn't exist anymore")` |
| **Async queue full (overrun_oldest policy)** | No exception — oldest message is overwritten, `overrun_counter_++` |
| **Async queue full (discard_new policy)** | No exception — new message is discarded, `discard_counter_++` |
| **Background thread sink exception** | `SPDLOG_LOGGER_CATCH` in `backend_sink_it_()` catches → `err_handler_` called → background thread continues running, does not crash |
| **Unknown exception (`catch(...)` rethrow)** | If custom `err_handler_` is not set: default `err_handler_` is called, then the rethrown exception escapes to the calling thread — **this is the only exception path that can propagate to user code** |

### `SPDLOG_NO_EXCEPTIONS` Mode

```cpp
#define SPDLOG_THROW(ex) do { printf("spdlog fatal error: %s\n", ex.what()); std::abort(); } while(0)
```

All exception paths become `printf` + `abort()` — logging library failure directly terminates the process. Suitable for embedded or RTTI-disabled environments.

## Iterator / Reference Invalidation

spdlog's "iterator invalidation" scenarios do not involve container iterators, but rather **message object lifetime, format buffer reuse, and safe windows for sink list modification**.

### `log_msg` `string_view` Lifetime

`log_msg::payload` and `log_msg::logger_name` are both `string_view_t` — they do not own data.

| Path | payload points to | Valid period |
|------|------------------|-------------|
| Synchronous logger | Stack-allocated `memory_buf_t buf` in `log_()` | From `vformat_to` to `sink_it_` return — each sink consumes within this window |
| Async logger | `log_msg_buffer::buffer` in `async_msg` (heap-allocated copy) | From enqueue to background thread `backend_sink_it_` completion — queue owns data |
| backtracer | Each `buffer` in `circular_q<log_msg_buffer>` | From `push_back` to being overwritten or `disable_backtrace()` |

### Sink List Concurrent Safety

```cpp
// logger's sinks_ is std::vector<sink_ptr> — no internal lock
const std::vector<sink_ptr> &sinks() const;  // Returns reference
std::vector<sink_ptr> &sinks();              // Returns mutable reference
```

- **Synchronous logger**: `sinks()` returns a raw reference. If one thread is iterating `sinks_` in `sink_it_()` while another thread calls `sinks().push_back()` → **data race, undefined behavior**.
- **Async logger**: `backend_sink_it_()` executes in the background thread. User modifies the list via `sinks()` in the main thread → **same data race**.
- **Safe practice**: Complete sink configuration before registering the logger with the registry (i.e., during single-threaded initialization phase), and do not modify `sinks_` afterwards.

### Formatter Replacement Safe Window

```cpp
void logger::set_formatter(std::unique_ptr<formatter> f) {
  for (auto it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (std::next(it) == sinks_.end()) {
      (*it)->set_formatter(std::move(f));  // Last sink transfers ownership
    } else {
      (*it)->set_formatter(f->clone());    // Other sinks each clone
    }
  }
}
```

- `set_formatter` does a complete traversal of `sinks_` and replaces one by one — if during this time other threads are using the old formatter in `sink_it_()` → **data race**.
- Each sink's `formatter_` is a `unique_ptr`; replacement is destroy-old + hold-new — no iterator invalidation involved, but pointer invalidation occurs.

### `async_msg` Move-Only Semantics

`async_msg` has its copy constructor deleted, only allowing move. This ensures exclusive ownership of messages in the queue:

```
Producer: async_msg am(...) → q_.enqueue(std::move(am))  <- am is inaccessible afterwards
Consumer: q_.dequeue(msg) → msg now exclusively owned by consumer → backend_sink_it_(msg) → msg destroyed
```

## Performance Model

### Formatting Cost

```
Hot path cost breakdown (synchronous logger, single sink):

1. Level check        → 1 atomic load (relaxed) ≈ 1 ns
2. Formatting          → fmt::vformat_to + pattern_formatter::format
   - Simple message (1 argument): ≈ 50-100 ns (depends on argument type and pattern complexity)
   - Complex message (multiple arguments + timestamp formatting): ≈ 200-500 ns
3. Sink write         → depends on sink type:
   - stdout_sink: ≈ 100-500 ns (affected by terminal buffering)
   - basic_file_sink: ≈ 200-1000 ns (affected by filesystem and OS cache)
   - rotating_file_sink: additional size check overhead ≈ 50 ns
```

### Compile-Time Pruning Code Generation Effect

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
void foo() {
  spdlog::trace("expensive: {}", compute());  // Eliminated at compile time
  spdlog::info("value: {}", 42);              // Retained
}
```

`SPDLOG_LEVEL_TRACE (0) > SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO (2)` → macro expands to `if (0 <= 2)` → always false → entire call (including `compute()` argument evaluation) is eliminated by the compiler. No code is generated for `compute()` (assuming no side effects).

### Sink Fan-Out Serial Bottleneck

The logger's `sinks_` iteration is **serial**:

```
sink_it_(msg):
  for sink in sinks_:
    sink->log(msg)    <- accumulated latency
```

Total latency for N sinks = Σ(sink_i.log latency). If one sink blocks (e.g. TCP sink network delay), all subsequent sinks are delayed.

### Async Mode Queue Contention

```
Producer (calling thread):
  block policy: condition variable wait ≈ several μs (no contention) to several ms (queue full)
  overrun_oldest: mutex lock + push_back ≈ 100-200 ns
  discard_new: mutex lock + check + push/discard ≈ 100-200 ns

Consumer (background thread):
  dequeue: mutex lock + pop_front ≈ 100-200 ns
  + sink write cost

Block policy backpressure when queue is full:
  → producer thread suspended (futex/condvar wait)
  → wake-up latency ≈ 1-10 μs (depends on OS scheduling)
  → can become a bottleneck under high load — increase queue capacity or switch to overrun_oldest policy
```

### Background Thread Batch Writing

The background thread `worker_loop_` is a single-consumer loop:

```cpp
while (process_next_msg_()) { }  // Consumes one by one, no batch optimization
```

There is no drain-all or batch-write optimization — each message independently dequeues + sinks. In high-throughput scenarios, the per-message mutex lock/unlock + condvar notify is the main overhead.

### Memory Cost

| Component | Size |
|-----------|------|
| `log_msg` | ~80 bytes (multiple string_views + time_point + source_loc) |
| `log_msg_buffer` | ~80 + 250 bytes (inline `memory_buf_t` capacity) |
| `async_msg` | ~330 + 16 bytes (msg_type + shared_ptr worker_ptr) |
| Queue capacity 8192 | 8192 × ~350 ≈ 2.7 MB pre-allocated |

## libstdc++ vs libc++ vs MSVC

spdlog is a cross-platform library, but differences across standard library implementations affect its behavior:

### fmt Library vs std::format Choice

| Configuration | Formatting Backend | Behavior Difference |
|--------------|-------------------|-------------------|
| Default (no `SPDLOG_USE_STD_FORMAT`) | Built-in fmt library | Consistent behavior — fmt is spdlog's submodule |
| `SPDLOG_USE_STD_FORMAT` | `std::format` / `std::vformat_to` | Depends on standard library implementation |

### `std::format` Implementation Differences (When `SPDLOG_USE_STD_FORMAT` is Enabled)

| Dimension | libstdc++ (GCC) | libc++ (Clang) | MSVC STL |
|-----------|----------------|----------------|----------|
| `std::format_string` availability | GCC 13+ / `__cpp_lib_format >= 202207L` | Clang 17+ | VS 2022 17.6+ |
| `std::format` old version degradation | `format_string_t` degrades to `string_view` (no compile-time checking) | Same | Same |
| Chrono formatting | Some locale-related specifier behavior slightly differs | Same | `%Z` (timezone name) behavior may differ |
| Floating-point formatting precision boundary | Follows IEEE spec, edge cases may be inconsistent | Same | Same |

### `std::string` Implementation Differences Affecting spdlog

spdlog extensively uses `string_view_t` and `memory_buf_t`:

| Dimension | libstdc++ | libc++ | MSVC |
|-----------|-----------|--------|------|
| `string_view` construction/copy | 16 bytes (ptr + size), trivial | Same | Same |
| `std::string` SSO capacity | 15 bytes (32-byte object) | 22 bytes (24-byte object) | 15 bytes (32-byte object) |
| Impact on spdlog | `logger::name_` allocation behavior depends on string length | libc++'s larger SSO reduces heap allocation for short-name loggers | Same as libstdc++ |

### Thread Library Differences

| Dimension | libstdc++ | libc++ | MSVC |
|-----------|-----------|--------|------|
| `std::mutex` implementation | pthread_mutex_t wrapper | pthread_mutex_t wrapper | SRWLOCK (lightweight read-write lock) |
| `std::condition_variable` | pthread_cond_t wrapper | pthread_cond_t wrapper | CONDITION_VARIABLE |
| Impact on spdlog | `mpmc_blocking_queue`'s mutex + condvar performance may differ slightly on Windows | Same | SRWLOCK is extremely fast when uncontended (~20 ns lock/unlock) |
| `thread_local` support | Via `__thread` / `thread_local` | Same | `__declspec(thread)` / `thread_local`; MSVC 2013 does not support → `SPDLOG_NO_TLS` |

### spdlog's Built-in fmt Independence from Standard Library

In default mode, spdlog uses its own fmt submodule (`include/spdlog/fmt/bundled/`), completely bypassing the standard library's formatting implementation. This means:

- In default configuration, libstdc++ / libc++ / MSVC `std::format` differences **do not affect** spdlog at all.
- `memory_buf_t` = `fmt::basic_memory_buffer<char, 250>` rather than `std::string` — behavior is consistent across all platforms.
- Only when explicitly enabling `SPDLOG_USE_STD_FORMAT` do standard library formatting differences become a factor.

## Minimal Reproduction Code

```cpp
#include <spdlog/spdlog.h>

int main() {
  spdlog::info("value = {}", 42);
}
```

## Compile / Disassembly / Benchmark Evidence

### Synchronous Logger Hot Path Code Generation

With `SPDLOG_ACTIVE_LEVEL = SPDLOG_LEVEL_DEBUG`, key disassembly characteristics of `spdlog::info("msg {}", 42)` under GCC -O2:

```
Hot path instruction count (single sink, basic_file_sink):
  Level check:  ~3 instructions (mov + cmp + jcc)
  Formatting:   ~50-80 instructions (fmt::vformat_to expansion)
  Pattern formatting: ~30-50 instructions (timestamp + level + payload concatenation)
  File write:   write() syscall ≈ 1-5 μs (depends on OS cache)
```

### Code Generation After Compile-Time Pruning

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
spdlog::trace("hidden: {}", expensive_call());
```

Disassembly verification: `expensive_call()` and the entire `trace()` call are completely eliminated at `-O1` and above — **zero instructions, zero branches**. The macro expands to `if (0 <= 2)` → always false → dead code elimination.

### Async Logger Hot Path

```
Async enqueue hot path (overrun_oldest policy):
  Level check:  ~3 instructions
  Formatting:   ~50-80 instructions (same as synchronous — formatting in calling thread)
  async_msg construction: ~20 instructions (log_msg_buffer copies payload)
  mutex lock + push_back + notify: ~15-30 instructions
  Total:        ≈ 100-150 instructions (excluding formatting)
```

### Benchmark Reference Values

The following data is based on typical configuration (single thread, single sink); actual values are significantly affected by hardware and sink type:

| Scenario | Throughput Reference |
|----------|-------------------|
| Synchronous basic_file_sink, simple message | ~1-3 M msg/s |
| Synchronous stdout_color_sink | ~0.5-2 M msg/s (limited by terminal rendering) |
| Async overrun_oldest, basic_file_sink | ~5-15 M msg/s (formatting in calling thread, writing in background) |
| Async block policy, queue full | Throughput drops to background thread consumption rate |
| After compile-time pruning (trace level, active = info) | 0 msg/s (call eliminated) |

## cpplings Exercise Entry Points

- [`format1` — std::format formatting](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println formatted output](../../../exercises/cpp23/print23.cpp)
- [`jthread1` — std::jthread and stop_token](../../../exercises/cpp20/jthread1.cpp)
