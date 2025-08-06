#include "cctoml.h"
#include <iostream>
using namespace cctoml;

void test(){

}

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