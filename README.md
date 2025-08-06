# cctoml - 基于 C++17 的 TOML 库

## 概述

`cctoml` 是一个轻量级的 C++ 库，专为解析、操作和序列化 TOML 数据而设计。

`cctoml` 提供简单直观的 API，利用现代 C++ 特性（C++17 及以上）确保类型安全、性能和易用性。

`cctoml` 支持所有标准 TOML 数据类型（布尔值、数字、日期、字符串、数组和对象），并包括高级功能，如自定义类型序列化、异常处理和灵活的解析选项。

## 特性

- **仅头文件**：无需构建或链接外部库，只需包含 `cctoml.h` 和 `cctoml.cc`。
- **类型安全**：使用 `TomlValue` 类表示 TOML 数据，编译时检查类型正确性。
- **现代 C++**：利用 C++17 特性，如 `std::variant`、`std::string_view` 和 SFINAE，编写健壮且表达力强的代码。
- **自定义序列化**：通过 `toToml` 和 `fromToml` 函数支持用户定义类型，自动通过模板元编程检测。
- **灵活解析**：可配置的解析选项，支持非标准转义序列（`\x` 和 `\0`）。
- **异常处理**：提供 `TomlException` 和 `TomlParseException`，包含详细错误信息和解析错误的位置。
- **容器支持**：无缝序列化/反序列化 `std::vector`、`std::map` 和 `std::unordered_map`。
- **美化输出**：支持自定义缩进的 TOML 输出，便于阅读。
- **用户友好 API**：直观的操作符（`[]`、`=`）和方法（`get<T>`、`set`、`push_back`），简化 TOML 操作。
- **字面量支持**：使用 `_toml` 用户定义字面量直接解析 TOML 字符串。
- **编译器反射**：基于c++17的静态反射实现，用于注册序列化和反序列化结构体

## 要求

- C++17 或更高版本
- 标准 C++ 库
- 现代 C++ 编译器（如 GCC 7+、Clang 5+、MSVC 2017+）

## 安装

由于 `cctoml` 是仅头文件库，安装非常简单：

1. 下载或克隆仓库。
2. 将 `cctoml.h` 复制到项目的包含目录。
3. 在源文件中包含头文件：

```cpp
#include "cctoml.h"
```

## 使用

### 基本示例

```cpp
#include "cctoml.h"
#include <iostream>
using namespace cctoml;

int main() {
    // 创建 TOML 对象
    TomlValue toml = {{"name", "Alice"},
                      {"age", 25},
                      {"scores", {90, 85, 88}},
                      {"address", {{"city", "Wonderland"}, {"zip", "12345"}}}};

    // 打印 TOML
    std::cout << toml.toString(parser::StringifyType::TO_TOML, 2) << std::endl;

    // 访问和修改值
    toml["age"] = 26;
    toml["scores"].push_back(95);

    // 解析 TOML 字符串
    auto parsed = R"(name = "Bob"
age = 30)"_toml;
    std::cout << parsed.toString() << std::endl;

    // 获取类型化值
    auto name   = toml["name"].get<std::string>();
    int  age    = toml["age"].get<int>();
    auto scores = toml["scores"].get<std::vector<int>>();

    // 处理日期
    TomlDate date("2025-07-22T15:00:00Z");
    toml["birthday"] = date;

    // 序列化为 JSON
    std::cout << toml.toString(parser::StringifyType::TO_JSON, 2) << std::endl;

    return 0;
}
```

**输出**：

```toml
age = 25
name = "Alice"
scores = [90, 85, 88]

[address]
city = "Wonderland"
zip = "12345"

age = 30
name = "Bob"
```

```json
{
  "address": {
    "city": "Wonderland",
    "zip": "12345"
  },
  "age": 26,
  "birthday": "2025-07-22T15:00:00Z",
  "name": "Alice",
  "scores": [
    90,
    85,
    88,
    95
  ]
}
```

### 自定义类型序列化

```cpp
struct Person {
    std::string name;
    int age;
};

// 定义 toToml 用于序列化
cctoml::TomlValue toToml(const Person& p) {
    return cctoml::TomlValue({
        {"name", p.name},
        {"age", p.age}
    });
}

// 定义 fromToml 用于反序列化
void fromToml(const cctoml::TomlValue& toml, Person& p) {
    p.name = toml["name"].get<std::string>();
    p.age = toml["age"].get<int>();
}

int main() {
    using namespace cctoml;
    Person p{"Bob", 30};
    TomlValue toml = toToml(p);
    std::cout << toml.toString() << std::endl;

    Person p2;
    fromToml(toml, p2); // 反序列化
}
```

## API 亮点

### `TomlValue` 类

- 支持所有 TOML 类型的构造函数（布尔值、数字、日期、字符串、数组、对象）。
- 操作符：`[]` 用于访问元素，`=` 用于赋值。
- 方法：`get<T>`、`set`、`push_back`、`toString`、`type`、`isObject` 等。
- const 迭代器：`begin()` 和 `end()` 用于遍历数组和对象。
- `parse`：解析 TOML 字符串，支持自定义选项。
- `stringify`：将 `TomlValue` 序列化为 TOML 字符串，支持可选缩进。

### 异常

- `TomlException`：通用 TOML 错误（如类型不匹配）。
- `TomlParseException`：解析错误，包含位置信息。

## 许可证

`cctoml` 采用 MIT 许可证。详情见 `LICENSE` 文件。