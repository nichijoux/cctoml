#include <cctoml.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace cctoml;

class TomlStringifyer {
  public:
    static std::string stringify(const TomlValue& value, int indent = 0) {
        std::ostringstream oss;
        stringifyValue(value, oss, indent, 0);
        return oss.str();
    }

  private:
    static void
    stringifyValue(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
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
                std::string type;
                switch (value.asDate().type()) {
                    case TomlDate::TomlDateTimeType::INVALID: break;
                    case TomlDate::TomlDateTimeType::OFFSET_DATE_TIME: type = "datetime"; break;
                    case TomlDate::TomlDateTimeType::LOCAL_DATE_TIME:
                        type = "datetime-local";
                        break;
                    case TomlDate::TomlDateTimeType::LOCAL_DATE: type = "date-local"; break;
                    case TomlDate::TomlDateTimeType::LOCAL_TIME: type = "time-local"; break;
                }
                stringifyLeaf(type, value.asDate().toString(), oss, indent, level);
                break;
            }
        }
#undef STRINGIFY
    }

    inline static void stringifyLeaf(const std::string&  type,
                                     const std::string&  value,
                                     std::ostringstream& oss,
                                     int                 indent,
                                     int                 level) {
        oss << "{";
        if (indent > 0)
            oss << "\n" << std::string((level + 1) * indent, ' ');

        oss << R"("type": ")" << type << R"(",)";

        if (indent > 0)
            oss << "\n" << std::string((level + 1) * indent, ' ');
        oss << R"("value": )";
        stringifyString(value, oss);

        if (indent > 0)
            oss << "\n" << std::string(level * indent, ' ');
        oss << "}";
    }

    inline static void
    stringifyBoolean(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
        stringifyLeaf("bool", value.get<bool>() ? "true" : "false", oss, indent, level);
    }

    inline static void
    stringifyInteger(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
        auto num = value.get<int64_t>();
        if (std::isfinite(num)) {
            stringifyLeaf("integer", std::to_string(num), oss, indent, level);
        } else {
            throw TomlException("Cannot stringify infinite or NaN number");
        }
    }

    inline static void
    stringifyDouble(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
        auto num = value.get<double>();
        if (std::isinf(num)) {
            std::string v;
            if (std::signbit(num)) {
                v = "-inf";
            } else {
                v = "inf";
            }
            stringifyLeaf("float", v, oss, indent, level);
        } else if (std::isnan(num)) {
            stringifyLeaf("float", "nan", oss, indent, level);
        } else {
            std::ostringstream vss;
            vss << std::setprecision(std::numeric_limits<double>::max_digits10) << num;
            stringifyLeaf("float", vss.str(), oss, indent, level);
        }
    }

    inline static void
    stringifyString(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
        stringifyLeaf("string", value.get<std::string>(), oss, indent, level);
    }

    static void stringifyString(const std::string& value, std::ostringstream& oss) {
        oss << '"';
        for (char c : value) {
            auto uc = static_cast<unsigned char>(c);  // 处理负值字符
            // 特殊处理已定义的转义字符
            switch (c) {
                case '"': oss << "\\\""; continue;
                case '\\': oss << "\\\\"; continue;
                case '\b': oss << "\\b"; continue;
                case '\f': oss << "\\f"; continue;
                case '\n': oss << "\\n"; continue;
                case '\r': oss << "\\r"; continue;
                case '\t': oss << "\\t"; continue;
                default: break;
            }

            // 处理其他控制字符（U+0000至U+0008、U+000A至U+001F、U+007F）
            if ((uc <= 0x08) || (uc >= 0x0A && uc <= 0x1F) || uc == 0x7F) {
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(uc);
                continue;
            }

            // 非控制字符直接输出
            oss << c;
        }
        oss << '"';
    }

    static void
    stringifyArray(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
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

    static void
    stringifyObject(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
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
};

int main() {
    // 使用stringstream读取所有输入
    std::stringstream buffer;
    buffer << std::cin.rdbuf();

    // 检查是否因错误而结束
    if (std::cin.bad() || std::cout.bad()) {
        std::cerr << "读写标准输入输出时发生错误" << std::endl;
        return 1;
    }
    std::string tomlStr = buffer.str();
    try {
        TomlValue toml = parser::Parse(tomlStr);
        std::cout << TomlStringifyer::stringify(toml, 4) << std::endl;
        return 0;
    } catch (const std::runtime_error& e) {
        std::cout << e.what() << std::endl;
        std::ofstream ofstream("error", std::ios::app);
        ofstream.write(tomlStr.c_str(), static_cast<std::streamsize>(tomlStr.size()));
        ofstream.write("\n", 1);
        ofstream.write(e.what(), static_cast<std::streamsize>(strlen(e.what())));
        ofstream.flush();
        ofstream.close();
        return 1;
    }
}