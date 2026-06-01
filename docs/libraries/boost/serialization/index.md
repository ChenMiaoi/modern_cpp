# Boost 序列化

## Serialization

Boost.Serialization 提供对象的持久化/反序列化，支持多种存档格式：

```cpp
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>

struct Employee {
    int id;
    std::string name;
    std::vector<std::string> skills;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & id & name & skills;
    }
};

// 序列化
std::ofstream ofs("data.txt");
boost::archive::text_oarchive oa(ofs);
oa << employee;

// 反序列化
std::ifstream ifs("data.txt");
boost::archive::text_iarchive ia(ifs);
ia >> employee;
```

支持的存档格式：text、binary、XML、JSON。支持版本化（schema evolution）——添加字段后仍可读取旧数据。

## PropertyTree

PropertyTree 是树形键值存储，适合配置文件和简单 XML/JSON/INI 解析：

```cpp
boost::property_tree::ptree pt;
pt.put("server.host", "localhost");
pt.put("server.port", 8080);

boost::property_tree::read_json("config.json", pt);
int port = pt.get<int>("server.port");
```

**与 JSON 的区别**：PropertyTree 处理的是简单的树形键值结构，不支持 JSON 数组。Boost.JSON 适合需要完整 JSON 支持的场景。
