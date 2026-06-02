---
title: "Boost 工具库"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Utility Libraries

## Filesystem

`boost::filesystem` is the predecessor of `std::filesystem` (C++17). Before C++17, it was the standard solution for cross-platform filesystem operations. Modern projects should use `std::filesystem` directly.

## UUID

```cpp
boost::uuids::random_generator gen;
boost::uuids::uuid id = gen();
std::cout << boost::uuids::to_string(id);  // "550e8400-e29b-41d4-a716-446655440000"
```

## Endian

Byte order conversion — essential in network protocols and binary file formats:

```cpp
boost::endian::big_uint32_t network_order = 42;  // Automatically converts to big-endian
uint32_t host_value = network_order;  // Automatically converts back to host byte order
```

## ProgramOptions

Command-line argument parsing:

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

Cross-platform dynamic library loading — a C++ wrapper for `dlopen`/`LoadLibrary`:

```cpp
boost::dll::shared_library lib("plugin.dll");
auto func = lib.get<int(int)>("process");
int result = func(42);
```

## Log

Boost.Log provides a complete logging framework:

- **Backends**: file, console, syslog, Windows Event Log
- **Filtering**: based on severity level, attributes, expressions
- **Formatting**: custom formats, timestamps, thread IDs
- **Async mode**: queued log writing

## Test

Boost.Test is a C++ unit testing framework:

```cpp
BOOST_AUTO_TEST_CASE(test_addition) {
    BOOST_CHECK_EQUAL(1 + 1, 2);
    BOOST_REQUIRE(result != nullptr);
    BOOST_CHECK_THROW(func(), std::runtime_error);
}
```
