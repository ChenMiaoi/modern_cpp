---
title: "Boost 网络与 I/O"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Networking & I/O

## Asio: Complete Implementation of the Proactor Model

Asio is the most widely used library in Boost and the de facto standard for C++ network programming.

### Proactor vs Reactor

| Model | Notification Timing | Typical Implementation |
|-------|---------------------|------------------------|
| Reactor | Ready (readable/writable) | libevent, libuv |
| **Proactor** | **Completion** (data read/written) | **Asio, IOCP (Windows)** |

On Linux, Asio is essentially **Reactor + emulated Proactor**: epoll reports "readable" → Asio internally performs a non-blocking `read()` → only after data is ready does it invoke the user's completion handler.

### io_context Event Loop

```cpp
class io_context {
    detail::epoll_reactor  reactor_;   // Linux: epoll
    detail::scheduler& scheduler_;     // completion handler queue
    std::atomic<std::uint64_t> outstanding_work_{0};
public:
    std::size_t run();   // blocking run
    std::size_t poll();  // non-blocking
    void stop();         // force stop
};
```

### strand Serialization

```cpp
auto strand = asio::make_strand(io_ctx);
asio::post(strand, [&data] { data.push_back(1); });
asio::post(strand, [&data] { data.push_back(2); });
// Guarantees strict serialization, no manual locking needed by the user
```

`strand` does not bind to a thread — it only guarantees logical serialization. Internally it maintains a separate handler queue and schedules tasks serially by "draining" them.

### C++20 Coroutine Integration

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

How `use_awaitable` works: the async operation returns an `awaitable<T>`, whose `operator co_await()` returns an `awaiter`. `await_suspend()` saves the coroutine frame and initiates the async operation. When the operation completes, `coroutine_handle.resume()` resumes the coroutine.

---

## Beast: HTTP/WebSocket Protocol Engine

Built on top of Asio, exposing protocol-level abstractions:

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

Beast's design philosophy: **zero-overhead abstraction** — HTTP header fields use enum lookups instead of string comparisons, and `flat_buffer` uses a single contiguous memory block to avoid fragmentation.

---

## JSON: DOM and SAX Parsing

Boost.JSON provides two parsing modes:

- **DOM mode**: Parses the entire input into an in-memory tree structure (`value` / `object` / `array`)
- **SAX mode**: Event-driven incremental parsing (`basic_parser` callbacks)

```cpp
boost::json::value jv = boost::json::parse(R"({"name":"Alice","scores":[95,87]})");
std::string name = jv.at("name").as_string();
int first_score = jv.at("scores").at(0).as_int64();
```

Key optimization: `monotonic_resource` memory pool — all nodes of the JSON tree are allocated in a single memory block, and the entire block is freed at once upon destruction.

---

## URL: RFC 3986 Parsing

Boost.URL implements the full RFC 3986 URI specification, supporting independent access and modification of authority, path, query, and fragment components.
