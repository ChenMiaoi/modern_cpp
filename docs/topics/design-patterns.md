# C++ 设计模式

## 单例模式（Singleton）

```cpp
// Meyer's Singleton — C++11 线程安全
class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }
    void log(std::string_view msg) {
        std::lock_guard lock(mtx_);
        std::cerr << msg << '\n';
    }
private:
    Logger() = default;
    std::mutex mtx_;
};

// std::call_once — 需要显式初始化参数的场景
class Database {
    static std::unique_ptr<Database> instance_;
    static std::once_flag init_flag_;
public:
    static Database& instance() {
        std::call_once(init_flag_, [] {
            instance_.reset(new Database("localhost", 5432));
        });
        return *instance_;
    }
};
```

## 观察者模式（Signal-Slot）

```cpp
template <typename... Args>
class Signal {
    using Slot = std::function<void(Args...)>;
    std::vector<std::shared_ptr<Slot>> slots_;
public:
    Connection connect(Slot slot) {
        auto ptr = std::make_shared<Slot>(std::move(slot));
        slots_.push_back(ptr);
        return Connection(ptr);
    }
    void emit(Args... args) {
        for (auto& slot : slots_) if (*slot) (*slot)(args...);
    }
};

Signal<int, std::string> on_event;
on_event.connect([](int id, std::string msg) {
    std::cout << "Event " << id << ": " << msg << '\n';
});
on_event.emit(1, "startup");
```

## 策略模式（Strategy）

```cpp
// 方案 1: std::function — 最灵活，有堆分配
class Sorter {
    std::function<bool(int, int)> comp_;
public:
    explicit Sorter(std::function<bool(int, int)> c) : comp_(std::move(c)) {}
    void sort(std::vector<int>& data) { std::ranges::sort(data, comp_); }
};

// 方案 2: 模板参数 — 零开销，编译期绑定
template <typename Comp>
class StaticSorter {
    Comp comp_;
public:
    void sort(std::vector<int>& data) { std::ranges::sort(data, comp_); }
};

// 方案 3: std::variant — 无堆分配的有限策略集合
struct Ascending { bool operator()(int a, int b) const { return a < b; } };
struct Descending { bool operator()(int a, int b) const { return a > b; } };
using Policy = std::variant<Ascending, Descending>;
void sort_with_policy(std::vector<int>& data, Policy policy) {
    std::visit([&](auto& comp) { std::ranges::sort(data, comp); }, policy);
}
```

## 工厂模式（Factory）

```cpp
class Widget {
public:
    virtual ~Widget() = default;
    virtual void render() const = 0;
    virtual std::unique_ptr<Widget> clone() const = 0;
    static std::unique_ptr<Widget> create(const std::string& type);
};

class Button : public Widget {
    std::string label_;
public:
    explicit Button(std::string l) : label_(std::move(l)) {}
    void render() const override { std::cout << "Button: " << label_ << '\n'; }
    std::unique_ptr<Widget> clone() const override {
        return std::make_unique<Button>(*this);
    }
};

std::unique_ptr<Widget> Widget::create(const std::string& type) {
    static const std::unordered_map<std::string,
        std::function<std::unique_ptr<Widget>(const std::string&)>> registry = {
        {"button", [](auto& s) { return std::make_unique<Button>(s); }},
    };
    if (auto it = registry.find(type); it != registry.end())
        return it->second(type);
    throw std::invalid_argument("Unknown widget: " + type);
}
```

## 建造者模式（Builder）

```cpp
class HttpRequest {
    std::string method_, url_, body_;
    int timeout_ms_ = 5000;
    // 注意：Builder 需要访问 HttpRequest 的私有构造函数
    // 因此将 Builder 声明为友元，或让 Builder 保存分散字段
public:
    HttpRequest(std::string method, std::string url, std::string body, int timeout)
        : method_(std::move(method)), url_(std::move(url)),
          body_(std::move(body)), timeout_ms_(timeout) {}

    class Builder {
        std::string method_ = "GET", url_, body_;
        int timeout_ms_ = 5000;
    public:
        Builder& method(std::string m)  { method_ = std::move(m); return *this; }
        Builder& url(std::string u)     { url_ = std::move(u); return *this; }
        Builder& body(std::string b)    { body_ = std::move(b); return *this; }
        Builder& timeout(int ms)        { timeout_ms_ = ms; return *this; }
        HttpRequest build() {
            if (url_.empty()) throw std::logic_error("URL required");
            return HttpRequest(std::move(method_), std::move(url_),
                               std::move(body_), timeout_ms_);
        }
    };
};

auto req = HttpRequest::Builder{}
    .url("https://api.example.com")
    .method("POST").body(R"({"key":"value"})").timeout(10000).build();
```

## CRTP 与 Deducing This

```cpp
// 经典 CRTP — 编译期静态多态
template <typename Derived>
class Shape {
public:
    void draw() const { static_cast<const Derived*>(this)->draw_impl(); }
    double area() const { return static_cast<const Derived*>(this)->area_impl(); }
};

class Circle : public Shape<Circle> {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    void draw_impl() const { std::cout << "Circle(r=" << r_ << ")\n"; }
    double area_impl() const { return 3.14159265 * r_ * r_; }
};

// C++23 deducing this — 取代 CRTP
class ShapeNew {
public:
    template <typename Self>
    void draw(this const Self& self) { self.draw_impl(); }
};
```



## 模式演进总览

| 模式 | 传统实现 | 现代 C++ 替代 |
|------|----------|--------------|
| Singleton | 双重检查锁 | Meyer's / `call_once` |
| Observer | 虚函数接口 | `std::function` + Signal |
| Strategy | 虚基类 | `std::function` / variant / 模板 |
| Iterator | 手写迭代器 | Ranges + 协程 |
| CRTP | 奇异递归模板 | deducing this（C++23） |
| Type Erasure | 手写虚表 | `std::any` / `std::function` |
