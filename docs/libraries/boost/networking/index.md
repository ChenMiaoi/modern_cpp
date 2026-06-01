# Boost 网络与 I/O

## Asio：Proactor 模型的完整实现

Asio 是 Boost 中使用最广泛的库，也是 C++ 网络编程的事实标准。

### Proactor vs Reactor

| 模型 | 通知时机 | 典型实现 |
|------|---------|---------|
| Reactor | 就绪（可读/可写） | libevent, libuv |
| **Proactor** | **完成**（数据已读/已写） | **Asio, IOCP (Windows)** |

Linux 上的 Asio 本质上是 **Reactor + 模拟 Proactor**：epoll 报告"可读" → Asio 内部执行非阻塞 `read()` → 数据就绪后才调用用户的 completion handler。

### io_context 事件循环

```cpp
class io_context {
    detail::epoll_reactor  reactor_;   // Linux: epoll
    detail::scheduler& scheduler_;     // completion handler 队列
    std::atomic<std::uint64_t> outstanding_work_{0};
public:
    std::size_t run();   // 阻塞运行
    std::size_t poll();  // 非阻塞
    void stop();         // 强制停止
};
```

### strand 串行化

```cpp
auto strand = asio::make_strand(io_ctx);
asio::post(strand, [&data] { data.push_back(1); });
asio::post(strand, [&data] { data.push_back(2); });
// 保证严格串行，无需用户手动加锁
```

`strand` 不绑定线程——它只保证逻辑串行性。内部维护独立的 handler 队列，通过"排空"任务串行调度。

### C++20 协程集成

```cpp
asio::awaitable<void> echo_session(tcp::socket socket) {
    char data[1024];
    while (true) {
        std::size_t n = co_await socket.async_read_some(
            asio::buffer(data), asio::use_awaitable);
        co_await async_write(socket, asio::buffer(data, n),
                             asio::use_awaitable);
    }
}
```

`use_awaitable` 工作原理：异步操作返回 `awaitable<T>`，其实现 `operator co_await()` 返回 `awaiter`。`await_suspend()` 保存协程帧并发起异步操作。操作完成时 `coroutine_handle.resume()` 恢复协程。

---

## Beast：HTTP/WebSocket 协议引擎

构建在 Asio 之上，暴露协议级别的抽象：

```cpp
beast::http::response<http::string_body> http_get(
    const std::string& host, const std::string& target)
{
    asio::io_context io_ctx;
    tcp::resolver resolver(io_ctx);
    beast::tcp_stream stream(io_ctx);

    auto results = resolver.resolve(host, "80");
    stream.connect(results);

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);
    return res;
}
```

Beast 的设计哲学：**零开销抽象**——HTTP 头字段使用枚举查找而非字符串比较，`flat_buffer` 使用单个连续内存块避免碎片化。

---

## JSON：DOM 与 SAX 解析

Boost.JSON 提供两种解析模式：

- **DOM 模式**：一次性解析为内存中的树结构（`value` / `object` / `array`）
- **SAX 模式**：事件驱动的增量解析（`basic_parser` 回调）

```cpp
boost::json::value jv = boost::json::parse(R"({"name":"Alice","scores":[95,87]})");
std::string name = jv.at("name").as_string();
int first_score = jv.at("scores").at(0).as_int64();
```

关键优化：`monotonic_resource` 内存池——JSON 树的所有节点在同一个内存块中分配，析构时一次性释放整块内存。

---

## URL：RFC 3986 解析

Boost.URL 实现完整的 RFC 3986 URI 规范，支持 authority、path、query、fragment 的独立访问和修改。
