# Boost 工具库

## Filesystem

`boost::filesystem` 是 `std::filesystem`（C++17）的前身。在 C++17 之前，它是跨平台文件系统操作的标准方案。现代项目应直接使用 `std::filesystem`。

## UUID

```cpp
boost::uuids::random_generator gen;
boost::uuids::uuid id = gen();
std::cout << boost::uuids::to_string(id);  // "550e8400-e29b-41d4-a716-446655440000"
```

## Endian

字节序转换——在网络协议和二进制文件格式中至关重要：

```cpp
boost::endian::big_uint32_t network_order = 42;  // 自动转换为大端
uint32_t host_value = network_order;  // 自动转回主机字节序
```

## ProgramOptions

命令行参数解析：

```cpp
namespace po = boost::program_options;
po::options_description desc("Allowed options");
desc.add_options()
    ("help", "produce help message")
    ("input", po::value<std::string>(), "input file")
    ("verbose", po::value<int>()->default_value(0), "verbosity level")
;

po::variables_map vm;
po::store(po::parse_command_line(argc, argv, desc), vm);
if (vm.count("input"))
    std::cout << vm["input"].as<std::string>();
```

## DLL

跨平台动态库加载——`dlopen`/`LoadLibrary` 的 C++ 封装：

```cpp
boost::dll::shared_library lib("plugin.dll");
auto func = lib.get<int(int)>("process");
int result = func(42);
```

## Log

Boost.Log 提供完整的日志框架：

- **后端**：文件、控制台、syslog、Windows Event Log
- **过滤**：基于严重级别、属性、表达式
- **格式化**：自定义格式、时间戳、线程 ID
- **异步模式**：队列化日志写入

## Test

Boost.Test 是 C++ 单元测试框架：

```cpp
BOOST_AUTO_TEST_CASE(test_addition) {
    BOOST_CHECK_EQUAL(1 + 1, 2);
    BOOST_REQUIRE(result != nullptr);
    BOOST_CHECK_THROW(func(), std::runtime_error);
}
```
