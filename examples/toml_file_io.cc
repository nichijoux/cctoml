#include <cctoml.h>
#include <fstream>
#include <iostream>

using namespace cctoml;

int main() {
    // 读取toml文件
    std::ifstream inputFile("config.toml", std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open config.Toml" << std::endl;
        return 1;
    }

    // 将文件内容读入字符串
    std::string s((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    inputFile.close();
    // 解析toml
    TomlValue toml;
    try {
        toml = parser::parse(s);
        toml = parser::parse(toml.toString());
        std::cout << "Successfully parsed config.toml" << std::endl;

        // 打印Toml内容
        std::cout << "\nToml Content:" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << toml.toString(cctoml::parser::TO_JSON, 4) << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        std::cout << toml << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "parse error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}