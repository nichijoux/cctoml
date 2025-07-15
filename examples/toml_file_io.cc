#include <cctoml.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

using namespace cctoml;

class TomlStringifyer {
  public:
    static std::string stringify(const TomlValue& value, int indent = 0);

  private:
    static void
    stringifyValue(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    inline static void
    stringifyNull(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    inline static void
    stringifyBoolean(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    inline static void
    stringifyInteger(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    inline static void
    stringifyDouble(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    inline static void
    stringifyString(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    static void stringifyString(const std::string& value, std::ostringstream& oss);

    static void
    stringifyArray(const TomlValue& value, std::ostringstream& oss, int indent, int level);

    static void
    stringifyObject(const TomlValue& value, std::ostringstream& oss, int indent, int level);
};

std::string TomlStringifyer::stringify(const TomlValue& value, int indent) {
    std::ostringstream oss;
    stringifyValue(value, oss, indent, 0);
    return oss.str();
}

void TomlStringifyer::stringifyValue(const TomlValue&    value,
                                     std::ostringstream& oss,
                                     int                 indent,
                                     int                 level) {
    // 根据类型调用不同的stringify
#define STRINGIFY(type)                                                                            \
    case TomlType::type: return stringify##type(value, oss, indent, level)
    switch (value.type()) {
        STRINGIFY(Boolean);
        STRINGIFY(Integer);
        STRINGIFY(Double);
        STRINGIFY(String);
        STRINGIFY(Array);
        STRINGIFY(Object);
        case TomlType::Date: {
            oss << value.asDate();
            break;
        }
    }
#undef STRINGIFY
}

void TomlStringifyer::stringifyBoolean(const cctoml::TomlValue& value,
                                       std::ostringstream&      oss,
                                       int,
                                       int) {
    oss << (value.get<bool>() ? "true" : "false");
}

void TomlStringifyer::stringifyInteger(const cctoml::TomlValue& value,
                                       std::ostringstream&      oss,
                                       int,
                                       int) {
    auto num = value.get<int64_t>();
    if (std::isfinite(num)) {
        oss << num;
    } else {
        throw TomlException("Cannot stringify infinite or NaN number");
    }
}

void TomlStringifyer::stringifyDouble(const cctoml::TomlValue& value,
                                      std::ostringstream&      oss,
                                      int,
                                      int) {
    auto num = value.get<double>();
    if (std::isfinite(num)) {
        oss << std::to_string(num);
    } else {
        throw TomlException("Cannot stringify infinite or NaN number");
    }
}

void TomlStringifyer::stringifyString(const cctoml::TomlValue& value,
                                      std::ostringstream&      oss,
                                      int                      indent,
                                      int                      level) {
    stringifyString(value.get<std::string>(), oss);
}

void TomlStringifyer::stringifyString(const std::string& value, std::ostringstream& oss) {
    oss << '"';
    for (char c : value) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;  // 正确转义 \n
            case '\r': oss << "\\r"; break;  // 正确转义 \r
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    oss << '"';
}

void TomlStringifyer::stringifyArray(const cctoml::TomlValue& value,
                                     std::ostringstream&      oss,
                                     int                      indent,
                                     int                      level) {
    const auto& array = value.get<TomlArray>();
    if (array.empty()) {
        oss << "[]";
        return;
    }
    oss << "[";
    for (size_t i = 0; i < array.size(); i++) {
        if (i > 0) {
            oss << ",";
        }
        if (indent != 0) {
            oss << "\n";
        }
        oss << std::string((level + 1) * indent, ' ');
        stringifyValue(array[i], oss, indent, level + 1);
    }
    if (indent != 0) {
        oss << "\n" << std::string(level * indent, ' ');
    }
    oss << "]";
}

void TomlStringifyer::stringifyObject(const cctoml::TomlValue& value,
                                      std::ostringstream&      oss,
                                      int                      indent,
                                      int                      level) {
    const auto& object = value.get<TomlObject>();
    if (object.empty()) {
        oss << "{}";
        return;
    }
    oss << "{";
    bool first = true;
    for (const auto& [key, val] : object) {
        if (!first) {
            oss << ',';
        }
        if (indent != 0) {
            oss << "\n";
        }
        oss << std::string((level + 1) * indent, ' ');
        first = false;
        stringifyString(key, oss);
        oss << ':';
        stringifyValue(val, oss, indent, level + 1);
    }
    if (indent != 0) {
        oss << "\n" << std::string(level * indent, ' ');
    }
    oss << '}';
}

int main() {
    try {
        // 读取toml文件
        std::ifstream input_file("config.toml");
        if (!input_file.is_open()) {
            std::cerr << "Error: Could not open twitter.Toml" << std::endl;
            return 1;
        }

        // 将文件内容读入字符串
        std::string Toml_str((std::istreambuf_iterator<char>(input_file)),
                             std::istreambuf_iterator<char>());
        input_file.close();

        // 解析toml
        TomlValue toml;
        try {
            toml = TomlParser::Parse(Toml_str);
            std::cout << "Successfully parsed config.toml" << std::endl;

            // 打印Toml内容
            std::cout << "\nToml Content:" << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            std::cout << TomlStringifyer::stringify(toml, 4) << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            std::cout << toml << std::endl;
        } catch (const TomlException& e) {
            std::cerr << "Parse error: " << e.what() << std::endl;
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}