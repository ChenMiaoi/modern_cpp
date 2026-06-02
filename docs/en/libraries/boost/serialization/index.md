---
title: "Boost 序列化"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Serialization

## Serialization

Boost.Serialization provides object persistence/deserialization with support for multiple archive formats:

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

// Serialization
std::ofstream ofs("data.txt");
boost::archive::text_oarchive oa(ofs);
oa << employee;

// Deserialization
std::ifstream ifs("data.txt");
boost::archive::text_iarchive ia(ifs);
ia >> employee;
```

Supported archive formats: text, binary, XML, JSON. Supports schema evolution — old data can still be read after adding new fields.

## PropertyTree

PropertyTree is a hierarchical key-value store, suitable for configuration files and simple XML/JSON/INI parsing:

```cpp
boost::property_tree::ptree pt;
pt.put("server.host", "localhost");
pt.put("server.port", 8080);

boost::property_tree::read_json("config.json", pt);
int port = pt.get<int>("server.port");
```

**Difference from JSON**: PropertyTree handles simple hierarchical key-value structures and does not support JSON arrays. Boost.JSON is suitable for scenarios that require full JSON support.
