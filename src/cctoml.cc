#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#include "cctoml.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace cctoml {

void TomlDate::parse(const std::string& s) {
    // 正则表达式捕获组索引:
    // 1: 年, 2: 月, 3: 日
    // 4: 时, 5: 分, 6: 秒
    // 7: 亚秒 (不包含'.')
    // 8: 时区 (Z, +, -)
    // 9: 时区时, 10: 时区分

    // 1. Offset Date-Time (最具体)
    static const std::regex offset_dt_re(
        R"((\d{4})-(\d{2})-(\d{2})[Tt ](\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,9}))?([Zz]|([+-])(\d{2}):(\d{2})))");
    std::smatch m;
    if (std::regex_match(s, m, offset_dt_re)) {
        m_type   = TomlDateTimeType::OFFSET_DATE_TIME;
        m_year   = std::stoi(m[1]);
        m_month  = std::stoi(m[2]);
        m_day    = std::stoi(m[3]);
        m_hour   = std::stoi(m[4]);
        m_minute = std::stoi(m[5]);
        m_second = std::stoi(m[6]);
        if (m[7].matched) {
            std::string sub = m[7].str();
            sub.resize(9, '0');  // 补全到纳秒精度
            m_subSecond = std::chrono::nanoseconds(std::stoll(sub));
        }
        if (m[8].str() == "Z" || m[8].str() == "z") {
            m_tzOffset = std::chrono::minutes(0);
        } else {
            int sign   = (m[9].str() == "+") ? 1 : -1;
            int h      = std::stoi(m[10]);
            int min    = std::stoi(m[11]);
            m_tzOffset = std::chrono::minutes(sign * (h * 60 + min));
        }
        return;
    }

    // 2. Local Date-Time
    static const std::regex local_dt_re(
        R"((\d{4})-(\d{2})-(\d{2})[Tt](\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,9}))?)");
    if (std::regex_match(s, m, local_dt_re)) {
        m_type   = TomlDateTimeType::LOCAL_DATE_TIME;
        m_year   = std::stoi(m[1]);
        m_month  = std::stoi(m[2]);
        m_day    = std::stoi(m[3]);
        m_hour   = std::stoi(m[4]);
        m_minute = std::stoi(m[5]);
        m_second = std::stoi(m[6]);
        if (m[7].matched) {
            std::string sub = m[7].str();
            sub.resize(9, '0');
            m_subSecond = std::chrono::nanoseconds(std::stoll(sub));
        }
        return;
    }

    // 3. Local Date
    static const std::regex local_d_re(R"((\d{4})-(\d{2})-(\d{2}))");
    if (std::regex_match(s, m, local_d_re)) {
        m_type  = TomlDateTimeType::LOCAL_DATE;
        m_year  = std::stoi(m[1]);
        m_month = std::stoi(m[2]);
        m_day   = std::stoi(m[3]);
        return;
    }

    // 4. Local Time
    static const std::regex local_t_re(R"((\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,9}))?)");
    if (std::regex_match(s, m, local_t_re)) {
        m_type   = TomlDateTimeType::LOCAL_TIME;
        m_hour   = std::stoi(m[1]);
        m_minute = std::stoi(m[2]);
        m_second = std::stoi(m[3]);
        if (m[4].matched) {
            std::string sub = m[4].str();
            sub.resize(9, '0');
            m_subSecond = std::chrono::nanoseconds(std::stoll(sub));
        }
        return;
    }

    throw std::invalid_argument("String does not match any valid TOML date/time format: " + s);
}

std::string TomlDate::toString() const {
    std::ostringstream oss;
    oss << std::setfill('0');

    if (m_year.has_value()) {
        oss << std::setw(4) << *m_year << '-' << std::setw(2) << *m_month << '-' << std::setw(2)
            << *m_day;
        if (m_hour.has_value()) {
            oss << 'T';
        }
    }

    if (m_hour.has_value()) {
        oss << std::setw(2) << *m_hour << ':' << std::setw(2) << *m_minute << ':' << std::setw(2)
            << *m_second;
    }

    if (m_subSecond.has_value() && m_subSecond->count() > 0) {
        std::string sub_str = std::to_string(m_subSecond->count());
        sub_str.insert(0, 9 - sub_str.length(), '0');
        // TOML spec: trim trailing zeros
        sub_str.erase(sub_str.find_last_not_of('0') + 1, std::string::npos);
        oss << '.' << sub_str;
    }

    if (m_tzOffset.has_value()) {
        if (m_tzOffset->count() == 0) {
            oss << 'Z';
        } else {
            long long offset_val = m_tzOffset->count();
            oss << (offset_val > 0 ? '+' : '-');
            offset_val = std::abs(offset_val);
            oss << std::setw(2) << offset_val / 60 << ':' << std::setw(2) << offset_val % 60;
        }
    }

    return oss.str();
}

std::chrono::system_clock::time_point TomlDate::toSystemTimePoint() const {
    if (!isOffsetDateTime()) {
        throw std::logic_error(
            "Only OffsetDateTime can be converted to a system_clock::time_point.");
    }

    std::tm tm  = {};
    tm.tm_year  = *m_year - 1900;
    tm.tm_mon   = *m_month - 1;
    tm.tm_mday  = *m_day;
    tm.tm_hour  = *m_hour;
    tm.tm_min   = *m_minute;
    tm.tm_sec   = *m_second;
    tm.tm_isdst = 0;  // Don't use daylight saving with UTC

    // timegm converts a struct tm (in UTC) to time_t
    std::time_t time = timegm(&tm);
    if (time == -1) {
        throw std::runtime_error("Failed to convert to time_t.");
    }

    auto timepoint = std::chrono::system_clock::from_time_t(time);

    // Add subseconds and subtract the offset to get the final UTC time_point
    if (m_subSecond.has_value()) {
        timepoint += *m_subSecond;
    }

    timepoint -= *m_tzOffset;

    return timepoint;
}

TomlValue::TomlValue(const TomlValue& other) : m_type(other.m_type) {
    switch (other.m_type) {
        case TomlType::Boolean: m_value.boolean = other.m_value.boolean; break;
        case TomlType::Integer: m_value.iNumber = other.m_value.iNumber; break;
        case TomlType::Double: m_value.dNumber = other.m_value.dNumber; break;
        case TomlType::String: m_value.string = new TomlString(*other.m_value.string); break;
        case TomlType::Date: m_value.date = new TomlDate(*other.m_value.date); break;
        case TomlType::Array: m_value.array = new TomlArray(*other.m_value.array); break;
        case TomlType::Object: m_value.object = new TomlObject(*other.m_value.object); break;
    }
}

TomlValue::TomlValue(TomlValue&& other) noexcept : m_type(other.m_type) {
    m_value              = other.m_value;
    other.m_type         = TomlType::Object;
    other.m_value.object = nullptr;
}

TomlValue& TomlValue::operator=(const TomlValue& other) {
    if (this != &other) {
        destroyValue();
        m_type = other.m_type;
        switch (m_type) {
            case TomlType::Boolean: m_value.boolean = other.m_value.boolean; break;
            case TomlType::Integer: m_value.iNumber = other.m_value.iNumber; break;
            case TomlType::Double: m_value.dNumber = other.m_value.dNumber; break;
            case TomlType::String: m_value.string = new std::string(*other.m_value.string); break;
            case TomlType::Date: m_value.date = new TomlDate(*other.m_value.date); break;
            case TomlType::Array: m_value.array = new TomlArray(*other.m_value.array); break;
            case TomlType::Object: m_value.object = new TomlObject(*other.m_value.object); break;
        }
    }
    return *this;
}

TomlValue& TomlValue::operator=(TomlValue&& other) noexcept {
    if (this != &other) {
        destroyValue();
        m_type               = other.m_type;
        m_value              = other.m_value;
        other.m_type         = TomlType::Object;
        other.m_value.object = nullptr;
    }
    return *this;
}

TomlValue::~TomlValue() {
    destroyValue();
}

TomlValue::operator bool() const {
    if (!isBoolean()) {
        throw TomlException("Cannot convert to bool");
    }
    return m_value.boolean;
}

TomlValue::operator int16_t() const {
    if (!isNumber()) {
        throw TomlException("Cannot convert to int16_t");
    }
    if (m_type == TomlType::Integer) {
        return static_cast<int16_t>(m_value.iNumber);
    }
    return static_cast<int16_t>(m_value.dNumber);
}

TomlValue::operator int32_t() const {
    if (!isNumber()) {
        throw TomlException("Cannot convert to int32_t");
    }
    if (m_type == TomlType::Integer) {
        return static_cast<int32_t>(m_value.iNumber);
    }
    return static_cast<int32_t>(m_value.dNumber);
}

TomlValue::operator int64_t() const {
    if (!isNumber()) {
        throw TomlException("Cannot convert to int64_t");
    }
    if (m_type == TomlType::Integer) {
        return m_value.iNumber;
    }
    return static_cast<int64_t>(m_value.dNumber);
}

TomlValue::operator float() const {
    if (!isNumber()) {
        throw TomlException("Cannot convert to float");
    }
    if (m_type == TomlType::Double) {
        return static_cast<float>(m_value.dNumber);
    }
    return static_cast<float>(m_value.iNumber);
}

TomlValue::operator double() const {
    if (!isNumber()) {
        throw TomlException("Cannot convert to double");
    }
    if (m_type == TomlType::Double) {
        return m_value.dNumber;
    }
    return static_cast<double>(m_value.iNumber);
}

TomlValue::operator std::string() const {
    if (!isString()) {
        throw TomlException("Cannot convert to string");
    }
    return *m_value.string;
}

std::string TomlValue::toString() const {
    return TomlParser::Stringify(*this);
}

void TomlValue::destroyValue() noexcept {
    switch (m_type) {
        // 动态分配的内存
        case TomlType::String:
            delete m_value.string;
            m_value.string = nullptr;
            break;
        case TomlType::Array:
            delete m_value.array;
            m_value.array = nullptr;
            break;
        case TomlType::Object:
            delete m_value.object;
            m_value.object = nullptr;
            break;
        default: break;
    }
}

TomlValue& TomlValue::set(const std::string& key, const TomlValue& value) {
    if (m_type != TomlType::Object) {
        destroyValue();
        m_type         = TomlType::Object;
        m_value.object = new TomlObject();
    }
    (*m_value.object)[key] = value;
    return *this;
}

TomlValue& TomlValue::push_back(const TomlValue& value) {
    if (!isArray()) {
        destroyValue();
        m_type        = TomlType::Array;
        m_value.array = new TomlArray();
    }
    m_value.array->emplace_back(value);
    return *this;
}

/**
 * @brief 跳过所有空白字符（空格/制表符/换行符等）
 * @param data 输入的字符串数据（可以是std::string或std::string_view）
 * @param position 当前读取位置（会被修改）
 */
#define SKIP_WHITESPACE(data, position)                                                            \
    do {                                                                                           \
        /* 边界检查 && 当前字符是空白 */                                                \
        while (position < data.size() && std::isspace(data[position])) {                           \
            position++; /* 移动到下一个字符 */                                             \
        }                                                                                          \
    } while (false)

/**
 * @brief 跳过空白字符和注释（直到行尾）
 * @param data 输入的字符串数据
 * @param position 当前读取位置（会被修改）
 *
 * @note 先跳过空白，再处理注释
 * @note 注释以#开始直到行尾（\\n或\\r）
 */
#define SKIP_USELESS_CHAR(data, position)                                                          \
    do {                                                                                           \
        while (position < data.size()) {                                                           \
            /* 1. 先跳过普通空白 */                                                         \
            SKIP_WHITESPACE(data, position);                                                       \
                                                                                                   \
            /* 2. 检查是否遇到注释 */                                                      \
            if (position < data.size() && data[position] == '#') {                                 \
                /* 跳过整行注释 */                                                           \
                while (position < data.size() && data[position] != '\n' &&                         \
                       data[position] != '\r') {                                                   \
                    position++;                                                                    \
                }                                                                                  \
            } else {                                                                               \
                /* 非注释字符，结束跳过 */                                               \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (false)

TomlValue TomlParser::Parse(std::string_view data) {
    size_t    position = 0;
    TomlValue root;
    // 按行解析
    size_t size = data.size();
    // 1. 先解析顶层内容
    SKIP_USELESS_CHAR(data, position);
    if (position < size && data[position] != '[') {
        std::vector<std::pair<std::vector<std::string>, TomlValue>> keyValues;
        while (position < data.size()) {
            SKIP_USELESS_CHAR(data, position);
            // 如果遇到新的table或者已经没有内容了
            if (position >= data.size() || data[position] == '[') {
                break;
            }
            // 解析key-value
            keyValues.emplace_back(ParseKeyValue(data, position));
            SKIP_USELESS_CHAR(data, position);
        }
        // 根据表头添加数据
        // 对于当前节点赋值
        for (const auto& [k, v] : keyValues) {
            TomlValue* node = &root;
            for (size_t i = 0; i < k.size() - 1; i++) {
                if (node->isObject()) {
                    node = &(*node)[k[i]];
                } else {
                    throw TomlException("node is not a object");
                }
            }
            if (node->isObject()) {
                node->set(k.back(), v);
            } else {
                throw TomlParseException("not a object", position);
            }
        }
    }
    // 2. 解析[]中的内容
    while (position < size) {
        // 跳过无用字符串
        std::vector<std::string> headers;
        bool                     isArray = false;
        SKIP_USELESS_CHAR(data, position);
        if (position < size && data[position] == '[') {
            if (position + 1 < size && data[position + 1] == '[') {
                // 数组类型
                isArray = true;
            }
            // 解析表头
            headers = ParseTableHeader(data, position, isArray);
        } else if (position >= size) {
            break;
        }
        // 解析下面的key-value
        std::vector<std::pair<std::vector<std::string>, TomlValue>> keyValues;
        while (position < data.size()) {
            SKIP_USELESS_CHAR(data, position);
            // 如果遇到新的table或者已经没有内容了
            if (position >= data.size() || data[position] == '[') {
                break;
            }
            // 解析key-value
            keyValues.emplace_back(ParseKeyValue(data, position));
        }
        // 根据表头添加数据
        TomlValue* node = &root;
        for (size_t i = 0; i < headers.size() - 1; i++) {
            if (node->isObject()) {
                node = &(*node)[headers[i]];
            } else if (node->isArray()) {
                auto& array = node->asArray();
                // 获取node的一个back
                if (array.empty()) {
                    // 创建一个object推入node中
                    TomlObject object;
                    object[headers[i]];
                    array.emplace_back(object);
                } else {
                    bool ok = false;
                    for (auto it = array.rbegin(); it != array.rend(); ++it) {
                        if (it->isObject()) {
                            ok = true;
                            if (it->asObject().find(headers.back()) == it->asObject().end()) {
                                if (isArray) {
                                    it->set(headers.back(), TomlArray());
                                } else {
                                    it->set(headers.back(), TomlValue());
                                }
                            }
                            break;
                        }
                    }
                    if (!ok) {
                        TomlObject object;
                        object[headers.back()];
                        array.emplace_back(object);
                    }
                }
                node = &(node->asArray().back()[headers.back()]);
            }
        }
        // 是array
        if (isArray) {
            if (node->isObject() &&
                node->asObject().find(headers.back()) == node->asObject().end()) {
                node->set(headers.back(), TomlArray());
            }
        }
        // 如果不存在则会创建一个object
        if (node->isObject()) {
            node = &(*node)[headers.back()];
        } else if (node->isArray()) {
            // 获取node的一个back
            auto& array = node->asArray();
            if (array.empty()) {
                // 创建一个object推入node中
                TomlObject object;
                object[headers.back()];
                array.emplace_back(object);
            } else {
                // 数组不为空
                bool ok = false;
                for (auto it = array.rbegin(); it != array.rend(); ++it) {
                    if (it->isObject()) {
                        ok = true;
                        if (it->asObject().find(headers.back()) == it->asObject().end()) {
                            if (isArray) {
                                it->set(headers.back(), TomlArray());
                            } else {
                                it->set(headers.back(), TomlValue());
                            }
                        }
                        break;
                    }
                }
                if (!ok) {
                    TomlObject object;
                    object[headers.back()];
                    array.emplace_back(object);
                }
            }
            node = &(node->asArray().back()[headers.back()]);
        }
        if (node->isArray() != isArray) {
            throw TomlParseException("node is not a array", position);
        }
        // 对于当前节点赋值
        if (node->isArray()) {
            // node是一个数组
            if (keyValues.empty()) {
                node->push_back(TomlValue());
            } else {
                // 新的数组对象
                TomlValue parent;
                for (const auto& [ks, v] : keyValues) {
                    TomlValue* tempNode = &parent;
                    for (size_t i = 0; i < ks.size() - 1; i++) {
                        tempNode = &(*tempNode)[ks[i]];
                    }
                    tempNode->set(ks.back(), v);
                }
                node->push_back(parent);
            }
        } else {
            // 对象
            for (const auto& [ks, v] : keyValues) {
                auto tempNode = node;
                for (size_t i = 0; i < ks.size() - 1; i++) {
                    tempNode = &(*tempNode)[ks[i]];
                }
                tempNode->set(ks.back(), v);
            }
        }
    }
    // 跳过无用字符串
    SKIP_USELESS_CHAR(data, position);
    if (position != data.size()) {
        throw TomlParseException("Unexpected content after Toml value", position);
    }
    return root;
}

TomlValue TomlParser::ParseBoolean(const std::string_view& data, size_t& position) {
    // 当前字符一定为t或f
    if (data.substr(position, 4) == "true") {
        position += 4;
        return true;
    } else if (data.substr(position, 5) == "false") {
        position += 5;
        return false;
    }
    throw TomlParseException("Expected 'true' or 'false'", position);
}

std::vector<std::string>
TomlParser::ParseTableHeader(const std::string_view& data, size_t& position, bool isArray) {
    // 当前字符一定为[,跳过表头
    position++;
    position += isArray;
    std::vector<std::string> headers;
    auto                     size = data.size();
    SKIP_WHITESPACE(data, position);
    if (position < size && (data[position] == '"' || data[position] == '\'')) {
        headers.emplace_back(ParseQuotedKeys(data, position));
    } else if (position < size) {
        // 开头是bareKey
        while (position < size) {
            SKIP_WHITESPACE(data, position);
            if (position < size && (data[position] == '"' || data[position] == '\'')) {
                headers.emplace_back(ParseQuotedKeys(data, position));
            } else {
                headers.emplace_back(ParseBareKey(data, position, true));
            }
            // 跳过空白字符
            SKIP_WHITESPACE(data, position);
            // 检查是否有点分隔符
            if (position < size && data[position] == '.') {
                position++;
            } else {
                break;
            }
        }
    }
    position++;
    // 如果是数组则还需要移动一次
    position += isArray;
    SKIP_USELESS_CHAR(data, position);
    return headers;
}

TomlString TomlParser::ParseBareKey(const std::string_view& data, size_t& position, bool isTable) {
    auto start = position;
    while (position < data.size()) {
        char c = data[position];
        if (std::isalnum(c) || c == '_' || c == '-') {
            position++;
        } else if (std::isspace(c) || c == '.' || (isTable && c == ']')) {
            // 结束了
            return std::string(data.substr(start, position - start));
        } else {
            break;
        }
    }
    throw TomlParseException("Unexpected content after Toml value", position);
}

TomlString TomlParser::ParseQuotedKeys(const std::string_view& data, size_t& position) {
    // 当前字符串一定为"或者'
    // 跳过开头的"或者'
    char       end = data[position++];
    TomlString result;
    result.reserve(32);  // 初始预留空间，避免频繁扩容
    // 读取后面的字符串
    while (position < data.size()) {
        char c = data[position++];
        if (c == end) {
            return result;
        } else if (c == '\\') {
            // 如果c为\,说明遇到了转义字符
            if (position >= data.size()) {
                throw TomlParseException("Unexpected end of string", position);
            }
            // 转义字符后的字符
            c = data[position++];
            switch (c) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u':
                case 'U': result += ParseUnicodeString(data, position); break;
                default: throw TomlParseException("Invalid escape sequence", position);
            }
        } else {
            result += c;
        }
    }
    // 异常
    throw TomlParseException("Unexpected end of string", position);
}

std::pair<std::vector<TomlString>, TomlValue>
TomlParser::ParseKeyValue(const std::string_view& data, size_t& position) {
    std::vector<TomlString> keys;
    auto                    size = data.size();
    if (position < size && (data[position] == '"' || data[position] == '\'')) {
        keys.emplace_back(ParseQuotedKeys(data, position));
    } else if (position < size) {
        // 开头是bareKey
        while (position < size) {
            SKIP_WHITESPACE(data, position);
            if (position < size && (data[position] == '"' || data[position] == '\'')) {
                keys.emplace_back(ParseQuotedKeys(data, position));
            } else {
                keys.emplace_back(ParseBareKey(data, position));
            }
            // 跳过空白字符
            SKIP_WHITESPACE(data, position);
            // 检查是否有点分隔符
            if (position < size && data[position] == '.') {
                position++;
            } else {
                break;
            }
        }
    } else {
        return {};
    }
    // 跳过空白，直到遇到=
    SKIP_WHITESPACE(data, position);
    if (position >= size || data[position] != '=') {
        throw TomlParseException("Expect = after a key", position);
    } else {
        position++;
    }
    // 解析value
    auto value = ParseValue(data, position);
    SKIP_USELESS_CHAR(data, position);
    return {keys, value};
}

TomlValue TomlParser::ParseValue(const std::string_view& data, size_t& position) {
    SKIP_WHITESPACE(data, position);
    char c = data[position];
    if (c == '"' || c == '\'') {
        return ParseString(data, position);
    } else if (c == '+' || c == '-' || std::isdigit(c) || c == 'i' || c == 'n') {
        // 处理inf、nan、数字和日期
        return ParseNumberOrDate(data, position);
    } else if (c == 't' || c == 'f') {
        return ParseBoolean(data, position);
    } else if (c == '[') {
        return ParseArray(data, position);
    } else if (c == '{') {
        return ParseObject(data, position);
    } else {
        throw TomlParseException("invalid value", position);
    }
}

#define MATCH3(sv, pos, ch)                                                                        \
    ((pos) + 2 < (sv).size() && (sv)[pos] == (ch) && (sv)[(pos) + 1] == (ch) &&                    \
     (sv)[(pos) + 2] == (ch))
#define MATCH4(sv, pos, ch)                                                                        \
    ((pos) + 3 < (sv).size() && (sv)[pos] == (ch) && (sv)[(pos) + 1] == (ch) &&                    \
     (sv)[(pos) + 2] == (ch) && (sv)[(pos) + 3] == (ch))
#define MATCH5(sv, pos, ch)                                                                        \
    ((pos) + 4 < (sv).size() && (sv)[pos] == (ch) && (sv)[(pos) + 1] == (ch) &&                    \
     (sv)[(pos) + 2] == (ch) && (sv)[(pos) + 3] == (ch) && (sv)[(pos) + 4] == (ch))

#define MATCH6(sv, pos, ch)                                                                        \
    ((pos) + 5 < (sv).size() && (sv)[pos] == (ch) && (sv)[(pos) + 1] == (ch) &&                    \
     (sv)[(pos) + 2] == (ch) && (sv)[(pos) + 3] == (ch) && (sv)[(pos) + 4] == (ch) &&              \
     (sv)[(pos) + 5] == (ch))

TomlValue TomlParser::ParseString(const std::string_view& data, size_t& position) {
    // 判断是哪种类型
    char c = data[position];
    if (MATCH3(data, position, c)) {
        // " 基本字符串
        // """ 多行基本字符串
        // ' 字面字符串
        // ''' 多行字面字符串
        if (c == '"') {
            return ParseMultiBasicString(data, position);
        } else if (c == '\'') {
            return ParseMultiLiteralString(data, position);
        } else {
            throw TomlParseException("not a string", position);
        }
    } else {
        if (c == '"') {
            return ParseBasicString(data, position);
        } else if (c == '\'') {
            return ParseLiteralString(data, position);
        } else {
            throw TomlParseException("not a string", position);
        }
    }
}

TomlValue TomlParser::ParseNumber(const std::string_view& data, size_t& position) {
    size_t start = position;
    size_t size  = data.size();
    // 处理正负号
    bool isNegative = false;
    if (data[position] == '+' || data[position] == '-') {
        isNegative = (data[position] == '-');
        ++position;
    }
    // 处理inf和nan
    if (position + 2 < size && data.substr(position, 3) == "inf") {
        auto inf = std::numeric_limits<double>::infinity();
        position += 3;
        return isNegative ? -inf : inf;
    } else if (position + 2 < size && data.substr(position, 3) == "nan") {
        position += 3;
        return std::numeric_limits<double>::quiet_NaN();
    }
    // 检查 base
    int base = 10;
    if (position + 1 < size && data[position] == '0') {
        char c = data[position + 1];
        if (c == 'b' || c == 'B') {
            base = 2;
            position += 2;
        } else if (c == 'o' || c == 'O') {
            base = 8;
            position += 2;
        } else if (c == 'x' || c == 'X') {
            base = 16;
            position += 2;
        } else if (std::isdigit(c)) {
            throw TomlParseException("Leading zeros are not allowed", position);
        }
    }

    // 判断是否浮点（需要推迟到后面判断）
    bool   isFloat = false;
    size_t scan    = position;
    // 在当前进制下判断是否数字是否合法
    auto isValidDigit = [base](char c) -> bool {
        switch (base) {
            case 2: return c == '0' || c == '1';
            case 8: return c >= '0' && c <= '7';
            case 10: return std::isdigit(c);
            case 16: return std::isxdigit(c);
            default: break;
        }
        return false;
    };
    // 读取主数字部分（整数或浮点）
    while (scan < size && (isValidDigit(data[scan]) || data[scan] == '_')) {
        ++scan;
    }
    // 浮点检测（仅 base-10 支持）
    if (base == 10 && scan < size && data[scan] == '.') {
        isFloat = true;
        ++scan;
        bool hasFraction = false;
        while (scan < size && (std::isdigit(data[scan]) || data[scan] == '_')) {
            if (std::isdigit(data[scan]))
                hasFraction = true;
            ++scan;
        }
        if (!hasFraction) {
            throw TomlParseException("Expected digits after '.'", scan);
        }
    }
    // 指数部分（仅 base-10 支持）
    if (base == 10 && scan < size && (data[scan] == 'e' || data[scan] == 'E')) {
        isFloat = true;
        ++scan;
        if (scan < size && (data[scan] == '+' || data[scan] == '-'))
            ++scan;
        bool hasExpDigit = false;
        while (scan < size && (std::isdigit(data[scan]) || data[scan] == '_')) {
            if (std::isdigit(data[scan]))
                hasExpDigit = true;
            ++scan;
        }
        if (!hasExpDigit) {
            throw TomlParseException("Expected digits after exponent", scan);
        }
    }
    // 数字部分
    std::string raw(data.substr(position, scan - position));
    position = scan;

    // 检查下划线合法性
    if (raw.front() == '_' || raw.back() == '_') {
        throw TomlParseException("Underscore at start or end", start);
    }
    if (raw.find("__") != std::string::npos) {
        throw TomlParseException("Consecutive underscores not allowed", start);
    }
    // 去除下划线
    raw.erase(std::remove(raw.begin(), raw.end(), '_'), raw.end());

    if (isFloat) {
        double result  = 0;
        auto [ptr, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), result);
        if (ec != std::errc()) {
            throw TomlParseException("Invalid float", start);
        }
        return isNegative ? -result : result;
    } else {
        int64_t result = 0;
        auto [ptr, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), result, base);
        if (ec != std::errc()) {
            throw TomlParseException("Invalid integer", start);
        }
        return isNegative ? -result : result;
    }
}

TomlValue TomlParser::ParseNumberOrDate(const std::string_view& data, size_t& position) {
    size_t original = position;
    // 判断是否为日期
    if (LooksLikeDateTime(data, position)) {
        // 完整匹配
        size_t end = position;
        while (end < data.size() && (std::isalnum(data[end]) || data[end] == '-' ||
                                     data[end] == ':' || data[end] == 'T' || data[end] == 'Z' ||
                                     data[end] == '+' || data[end] == '.' || data[end] == ' ')) {
            ++end;
        }
        // 可能的日期字符串
        auto maybeDate = data.substr(position, end - position);
        // 判断是否为日期
        if (MatchFullDateTime(maybeDate)) {
            // todo:使用日期类型
            position = end;
            return TomlDate(maybeDate);
        }
    }
    // 回退并作为普通数值解析
    position = original;
    return ParseNumber(data, position);
}

TomlValue TomlParser::ParseArray(const std::string_view& data, size_t& position) {
    // 当前字符一定为[
    position++;
    // 解析以,分割的一个个value
    TomlArray array;
    while (position < data.size()) {
        SKIP_USELESS_CHAR(data, position);
        if (data[position] == ']') {
            position++;
            break;
        }
        auto value = ParseValue(data, position);
        array.emplace_back(value);
        SKIP_USELESS_CHAR(data, position);
        if (position < data.size() && data[position] == ',') {
            position++;
        }
    }
    return array;
}

TomlValue TomlParser::ParseObject(const std::string_view& data, size_t& position) {
    // 当前一定为{
    position++;
    TomlValue object;
    while (position < data.size()) {
        SKIP_USELESS_CHAR(data, position);
        if (data[position] == '}') {
            position++;
            break;
        }
        // 解析一个个key-value
        auto node    = &object;
        auto [ks, v] = ParseKeyValue(data, position);
        for (size_t i = 0; i < ks.size() - 1; i++) {
            if (node->isObject()) {
                node = &(*node)[ks[i]];
            } else {
                throw TomlException("node is not a object");
            }
        }
        if (node->isObject()) {
            node->set(ks.back(), v);
        } else {
            throw TomlParseException("not a object", position);
        }
        SKIP_USELESS_CHAR(data, position);
        if (position < data.size() && data[position] == ',') {
            position++;
        }
    }
    return object;
}

std::string TomlParser::ParseUnicodeString(const std::string_view& data, size_t& position) {
    auto parseHex = [&](int length) -> char32_t {
        if (position + length > data.size()) {
            throw TomlParseException("Unexpected end in Unicode escape", position);
        }
        std::string hex(data.substr(position, length));
        position += length;
        return static_cast<char32_t>(std::stoul(hex, nullptr, 16));
    };

    auto     hexLength = data[position++] == 'u' ? 4 : 8;
    char32_t codePoint = parseHex(hexLength);

    std::string result;
    if (codePoint <= 0x7F) {
        result += static_cast<char>(codePoint);
    } else if (codePoint <= 0x7FF) {
        result += static_cast<char>(0xC0 | (codePoint >> 6));
        result += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else if (codePoint <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (codePoint >> 12));
        result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else {
        result += static_cast<char>(0xF0 | (codePoint >> 18));
        result += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codePoint & 0x3F));
    }

    return result;
}

std::string TomlParser::ParseBasicString(const std::string_view& data, size_t& position) {
    ++position;

    std::string result;
    while (position < data.size()) {
        char ch = data[position];
        if (ch == '"') {
            ++position;
            return result;
        }
        if (ch == '\\') {
            ++position;
            if (position >= data.size()) {
                throw TomlParseException("Invalid escape", position);
            }

            char esc = data[position++];
            switch (esc) {
                case 'b': result += '\b'; break;
                case 't': result += '\t'; break;
                case 'n': result += '\n'; break;
                case 'f': result += '\f'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'u':
                case 'U':
                    --position;
                    result += ParseUnicodeString(data, position);
                    break;
                default:
                    throw TomlParseException("Unknown escape: \\" + std::string(1, esc), position);
            }
        } else {
            result += ch;
            ++position;
        }
    }

    throw std::runtime_error("Unterminated basic string");
}

std::string TomlParser::ParseMultiBasicString(const std::string_view& data, size_t& position) {
    // 此时当前字符串一定为"""
    position += 3;
    // 跳过开头的换行符（如果存在）
    if (position < data.size()) {
        if (data[position] == '\r') {
            ++position;
        }
        if (position < data.size() && data[position] == '\n') {
            ++position;
        }
    }
    std::string result;
    bool        inEscape = false;  // 标记是否处于转义状态
    while (position < data.size()) {
        // 遇到结束标识
        if (!inEscape && MATCH3(data, position, '"')) {
            // 已经遇到了三个引号,但却不一定结束
            // 将当前两个字符视为普通字符即可
            // 说明后面还有",当前data[position]其实可能被视为普通字符,所允许的最大也不过为2+3
            if (position + 3 < data.size() && data[position + 3] == '"') {
                // 将当前两个字符视为普通字符即可
                // 说明后面还有",当前data[position]其实可能被视为普通字符,所允许的最大也不过为2+3
                if (!(MATCH4(data, position, '"') || MATCH5(data, position, '"')) ||
                    MATCH6(data, position, '"')) {
                    throw TomlParseException("not allow 3 \" in multi-basic string", position);
                }
            } else {
                position += 3;
                return result;
            }
        }
        char c = data[position++];
        if (c == '\\' && !inEscape) {
            if (position < data.size() && (data[position] == '\r' || data[position] == '\n')) {
                // 遇到了toml语法中的\换行
                // 当一行的最后一个非空白字符是未被转义的
                // \ 时，它会连同它后面的所有空白（包括换行）一起被去除，直到下一个非空白字符或结束引号为止。
                // 处理行尾转义
                if (position < data.size() && data[position] == '\r') {
                    ++position;
                }
                if (position < data.size() && data[position] == '\n') {
                    ++position;
                }
                // 跳过行尾后的所有空白
                while (position < data.size() && std::isspace(data[position])) {
                    ++position;
                }
            } else {
                inEscape = true;  // 普通转义字符，下一个字符特殊处理
            }
        } else if (inEscape) {
            // 处理普通转义序列
            switch (c) {
                case 'b': result += '\b'; break;
                case 't': result += '\t'; break;
                case 'n': result += '\n'; break;
                case 'f': result += '\f'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'u':
                case 'U':
                    --position;  // 回退以解析Unicode
                    result += ParseUnicodeString(data, position);
                    break;
                default:
                    throw TomlParseException("Unknown escape in multi-line: \\" + std::string(1, c),
                                             position);
            }
            inEscape = false;  // 重置转义标记
        } else {
            // 普通字符
            result += c;
        }
    }
    throw TomlParseException("Unterminated multi-line basic string", position);
}

std::string TomlParser::ParseLiteralString(const std::string_view& data, size_t& position) {
    ++position;

    std::string result;
    while (position < data.size()) {
        char c = data[position];
        if (c == '\'') {
            ++position;
            return result;
        }
        result += c;
        ++position;
    }

    throw TomlParseException("Unterminated literal string", position);
}

std::string TomlParser::ParseMultiLiteralString(const std::string_view& data, size_t& position) {
    position += 3;
    // 跳过开头的换行符（如果存在）
    if (position < data.size()) {
        if (data[position] == '\r') {
            ++position;
        }
        if (position < data.size() && data[position] == '\n') {
            ++position;
        }
    }
    std::string result;
    while (position < data.size()) {
        if (MATCH3(data, position, '\'')) {
            // 已经遇到了三个引号,但却不一定结束
            // 将当前两个字符视为普通字符即可
            // 说明后面还有",当前data[position]其实可能被视为普通字符,所允许的最大也不过为2+3
            if (position + 3 < data.size() && data[position + 3] == '\'') {
                // 将当前两个字符视为普通字符即可
                // 说明后面还有",当前data[position]其实可能被视为普通字符,所允许的最大也不过为2+3
                if (!(MATCH4(data, position, '\'') || MATCH5(data, position, '\'')) ||
                    MATCH6(data, position, '\'')) {
                    throw TomlParseException("not allow 3 ' in multi-literal string", position);
                }
            } else {
                position += 3;
                return result;
            }
        }
        result += data[position++];
    }

    throw std::runtime_error("Unterminated multi-line literal string");
}
#undef SKIP_WHITESPACE
#undef SKIP_USELESS_CHAR
#undef MATCH3
#undef MATCH4
#undef MATCH5
#undef MATCH6

bool TomlParser::LooksLikeDateTime(const std::string_view& data, size_t position) {
    size_t remaining = data.size() - position;
    if (remaining < 10) {
        return false;
    }

    // 日期必须以4位年开头，例：1979-05-27
    if (!std::isdigit(data[position]) || !std::isdigit(data[position + 1]) ||
        !std::isdigit(data[position + 2]) || !std::isdigit(data[position + 3])) {
        return false;
    }
    if (data[position + 4] != '-') {
        return false;
    }
    if (!std::isdigit(data[position + 5]) || !std::isdigit(data[position + 6])) {
        return false;
    }
    if (data[position + 7] != '-') {
        return false;
    }
    if (!std::isdigit(data[position + 8]) || !std::isdigit(data[position + 9])) {
        return false;
    }
    return true;
}

bool TomlParser::MatchFullDateTime(const std::string_view& data) {
    static const std::regex dateTimePattern(
        R"(^\d{4}-\d{2}-\d{2}(?:[T ]\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?)?$)");
    return std::regex_match(std::string(data), dateTimePattern);
}

std::string TomlParser::Stringify(const TomlValue& value) {
    std::ostringstream oss;
    StringifyObject(value, oss);
    return oss.str();
}

void TomlParser::StringifyArray(const TomlValue& value, std::ostringstream& oss) {
    const auto& array = value.get<TomlArray>();
    oss << "[";
    for (size_t i = 0; i < array.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        const auto& item = array[i];

        if (item.type() == TomlType::Object) {
            StringifyInlineObject(item.get<TomlObject>(), oss);
        } else {
            StringifyValue(item, oss);
        }
    }
    oss << "]";
}

void TomlParser::StringifyInlineObject(const TomlObject& obj, std::ostringstream& oss) {
    oss << "{ ";
    bool first = true;
    for (const auto& [key, val] : obj) {
        if (!first) {
            oss << ", ";
        }
        first = false;
        // 输出key和value
        oss << '"' << key << "\" = ";
        StringifyValue(val, oss);
    }
    oss << " }";
}

bool TomlParser::IsArrayOfTables(const TomlValue& value) {
    if (value.type() != TomlType::Array) {
        return false;
    }
    const auto& array = value.get<TomlArray>();
    if (array.empty()) {
        return false;
    }
    return std::all_of(array.begin(), array.end(),
                       [](const TomlValue& item) { return item.type() == TomlType::Object; });
}

void TomlParser::StringifyObject(const TomlValue& value, std::ostringstream& oss) {
    if (!value.isObject()) {
        throw TomlException("Top-level value must be object");
    }
    StringifyObject(value.asObject(), oss, "");
}

void TomlParser::StringifyObject(const TomlObject&   object,
                                 std::ostringstream& oss,
                                 const std::string&  prefix) {
    // 🔹 1. 输出非 Object 的字段（顶层属性）
    for (const auto& [key, val] : object) {
        if (val.type() != TomlType::Object && !IsArrayOfTables(val)) {
            if (StringIsBareKey(key)) {
                oss << key << " = ";
            } else {
                oss << '"' << key << "\" = ";
            }
            StringifyValue(val, oss);
            oss << "\n";
        }
    }

    // 🔹 2. 输出子对象 [section] 和 表数组[[section]]
    for (const auto& [key, val] : object) {
        if (val.type() == TomlType::Object) {
            const auto& child = val.get<TomlObject>();
            std::string fullKey;
            if (prefix.empty()) {
                fullKey = key;
            } else {
                fullKey.append(prefix).append(".").append(key);
            }
            oss << "\n[" << fullKey << "]\n";
            StringifyObject(child, oss, fullKey);
        } else if (IsArrayOfTables(val)) {
            const auto& array = val.asArray();
            std::string fullKey;
            if (prefix.empty()) {
                fullKey = key;
            } else {
                fullKey.append(prefix).append(".").append(key);
            }
            for (const auto& elem : array) {
                oss << "\n[[" << fullKey << "]]\n";
                StringifyObject(elem.asObject(), oss, fullKey);
            }
        }
    }
}

void TomlParser::StringifyValue(const TomlValue& value, std::ostringstream& oss) {
#define STRINGIFY(type)                                                                            \
    case TomlType::type: return Stringify##type(value, oss)
    switch (value.type()) {
        STRINGIFY(Boolean);
        STRINGIFY(Integer);
        STRINGIFY(Double);
        STRINGIFY(String);
        STRINGIFY(Date);
        STRINGIFY(Array);
        STRINGIFY(Object);
    }
#undef STRINGIFY
}

void TomlParser::StringifyBoolean(const TomlValue& value, std::ostringstream& oss) {
    oss << (value.get<bool>() ? "true" : "false");
}

void TomlParser::StringifyInteger(const TomlValue& value, std::ostringstream& oss) {
    oss << value.get<int64_t>();
}

void TomlParser::StringifyDouble(const TomlValue& value, std::ostringstream& oss) {
    auto num = value.get<double>();
    if (std::isinf(num)) {
        oss << "inf";
    } else if (std::isnan(num)) {
        oss << "nan";
    } else {
        oss << std::to_string(num);
    }
}

bool TomlParser::StringIsBareKey(const std::string& s) {
    // 空字符串不是合法的barekey
    if (s.empty()) {
        return false;
    }

    // 第一个字符不能是数字
    if (std::isdigit(static_cast<unsigned char>(s.front()))) {
        return false;
    }

    // 使用std::all_of检查剩余字符是否符合要求
    return std::all_of(s.begin() + 1, s.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}

void TomlParser::StringifyString(const TomlValue& value, std::ostringstream& oss) {
    StringifyString(value.get<std::string>(), oss);
}

void TomlParser::StringifyString(const std::string& s, std::ostringstream& oss) {
    oss << '"';
    for (char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    oss << '"';
}

void TomlParser::StringifyDate(const TomlValue& value, std::ostringstream& oss) {
    oss << value.asDate();
}

}  // namespace cctoml
#pragma clang diagnostic pop