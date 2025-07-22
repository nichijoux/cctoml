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

    static std::string stringifyDouble(double num) {
        // 处理特殊值
        if (std::isnan(num)) {
            return "nan";
        } else if (std::isinf(num)) {
            return num > 0 ? "inf" : "-inf";
        } else {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());  // 确保使用点号作为小数点

            // 检查是否为整数值但需要表示为浮点数
            bool isIntegerValue =
                (num == std::floor(num)) && (std::abs(num) < 1e14);  // 避免大整数精度问题

            const double absValue = std::abs(num);

            // 决定使用常规表示法还是科学计数法
            bool useScientific = (absValue >= 1e6) || (absValue > 0 && absValue < 1e-4);

            if (useScientific) {
                // 科学计数法表示 - 先获取原始科学计数法字符串
                oss << std::scientific << std::setprecision(15) << num;
                std::string str = oss.str();

                // 解析科学计数法的各个部分
                size_t ePos = str.find_first_of("eE");
                if (ePos == std::string::npos) {
                    return str;  // 不是科学计数法格式，直接返回
                }

                // 分解为尾数和指数部分
                std::string mantissa = str.substr(0, ePos);
                std::string exponent = str.substr(ePos + 1);

                // 处理尾数部分
                // 确保小数点存在
                //            if (mantissa.find('.') == std::string::npos) {
                //                mantissa += ".0";
                //            }

                // 移除尾数部分无意义的零
                size_t lastNonZero = mantissa.find_last_not_of('0');
                if (lastNonZero != std::string::npos) {
                    if (mantissa[lastNonZero] == '.') {
                        mantissa.erase(lastNonZero);
                    }
                }

                // 处理指数部分
                int expValue = std::stoi(exponent);

                // 重新构建科学计数法字符串
                std::ostringstream result;
                result << mantissa << 'e' << expValue;  // 直接输出指数，不带+号和前导零

                return result.str();
            } else if (isIntegerValue) {
                // 对于整数值的浮点数，强制添加 .0
                oss << std::fixed << std::setprecision(1) << num;
                return oss.str();
            } else {
                // 常规表示法
                oss << std::setprecision(std::numeric_limits<double>::max_digits10);
                oss << num;

                std::string str = oss.str();
//
//
//                // 安全地移除末尾无意义的零
//                size_t dotPos = str.find('.');
//                if (dotPos != std::string::npos) {
//                    size_t lastNonZero = str.find_last_not_of('0');
//                    if (lastNonZero == dotPos) {
//                        // 保留小数点后一位零
//                        if (dotPos + 2 <= str.size()) {
//                            str.erase(dotPos + 2);
//                        } else {
//                            //                        str += "0";
//                        }
//                    } else if (lastNonZero != std::string::npos && lastNonZero + 1 < str.size()) {
//                        str.erase(lastNonZero + 1);
//                    }
//                }
                return str;
            }
        }
    }

    inline static void
    stringifyDouble(const TomlValue& value, std::ostringstream& oss, int indent, int level) {
        auto num = value.get<double>();
        stringifyLeaf("float", stringifyDouble(num), oss, indent, level);
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
        TomlValue toml = parser::parse(tomlStr);
        toml           = parser::parse(toml.toString());
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