#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#include "cctoml.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace cctoml {

TomlDate::TomlDate(const TomlDate& date) noexcept {
    m_type      = date.m_type;
    m_core      = date.m_core;
    m_subSecond = date.m_subSecond;
}

TomlDate::TomlDate(TomlDate&& date) noexcept {
    m_type      = date.m_type;
    m_core      = date.m_core;
    m_subSecond = date.m_subSecond;
    // 赋值原date
    date.m_type = TomlDateTimeType::INVALID;
    date.m_core = date.m_subSecond = 0;
}

TomlDate& TomlDate::operator=(const cctoml::TomlDate& date) noexcept = default;

TomlDate& TomlDate::operator=(cctoml::TomlDate&& date) noexcept {
    m_type      = date.m_type;
    m_core      = date.m_core;
    m_subSecond = date.m_subSecond;
    // 赋值原date
    date.m_type = TomlDateTimeType::INVALID;
    date.m_core = date.m_subSecond = 0;
    return *this;
}

#define IS_DIGIT(c) ('0' <= (c) && (c) <= '9')

bool TomlDate::parseTimePart(const std::string_view& sv, size_t& position) {
    // 解析时间部分 hh:mm:ss
    auto hour = ParseDigits(sv, position, 2);
    if (!hour || *hour > 23 || position >= sv.size() || sv[position] != ':') {
        return false;
    }
    position++;
    // 解析分钟
    auto minute = ParseDigits(sv, position, 2);
    if (!minute || *minute > 59 || position >= sv.size() || sv[position] != ':') {
        return false;
    }
    position++;
    // 解析秒
    auto second = ParseDigits(sv, position, 2);
    if (!second || *second > 59) {
        return false;
    }

    // 存储时间部分
    setHour(*hour);
    setMinute(*minute);
    setSecond(*second);
    m_type = TomlDateTimeType::LOCAL_DATE_TIME;

    // 解析可选的亚秒部分
    if (position < sv.size() && sv[position] == '.') {
        position++;
        parseSubSecond(sv, position);
    }

    // 解析可选的时区偏移
    if (position < sv.size()) {
        parseTimezoneOffset(sv, position);
    }

    // 必须完全匹配整个字符串
    return position == sv.size();
}

void TomlDate::parseSubSecond(const std::string_view& sv, size_t& position) {
    const size_t fracStart = position;
    while (position < sv.size() && IS_DIGIT(sv[position])) {
        position++;
    }

    if (position == fracStart) {
        throw std::invalid_argument("'.' must be followed by digits in string: " + std::string(sv));
    }

    // 提取小数部分并转换为纳秒
    std::string sub(sv.substr(fracStart, position - fracStart));
    sub.resize(9, '0');  // 补零至9位(纳秒精度)

    int64_t subSecond = 0;
    auto [ptr, ec]    = std::from_chars(sub.data(), sub.data() + sub.size(), subSecond);
    if (ec != std::errc() && ptr != sub.data() + sub.size()) {
        throw std::invalid_argument("Invalid fractional second in string: " + std::string(sv));
    }

    m_subSecond = subSecond;
}

void TomlDate::parseTimezoneOffset(const std::string_view& sv, size_t& position) {
    const char offsetChar = sv[position];

    if (offsetChar == 'Z' || offsetChar == 'z') {
        position++;
        setTzOffset(0);
        m_type = TomlDateTimeType::OFFSET_DATE_TIME;
        return;
    }

    if (offsetChar != '+' && offsetChar != '-') {
        return;  // 没有时区信息，保持当前类型
    }

    position++;

    // 解析时区偏移 ±HH:MM
    auto offsetHour = ParseDigits(sv, position, 2);
    if (!offsetHour || *offsetHour > 23 || position >= sv.size() || sv[position] != ':') {
        throw std::invalid_argument("Invalid timezone offset hour in string: " + std::string(sv));
    }
    position++;

    auto offsetMinute = ParseDigits(sv, position, 2);
    if (!offsetMinute || *offsetMinute > 59) {
        throw std::invalid_argument("Invalid timezone offset minute in string: " + std::string(sv));
    }

    const int sign = (offsetChar == '+') ? 1 : -1;
    setTzOffset(sign * (*offsetHour * 60 + *offsetMinute));
    m_type = TomlDateTimeType::OFFSET_DATE_TIME;
}

std::optional<int> TomlDate::ParseDigits(const std::string_view& sv, size_t& position, int count) {
    if (position + count > sv.size()) {
        return std::nullopt;
    }

    int value = 0;
    for (int i = 0; i < count; ++i) {
        const auto c = sv[position + i];
        if (!IS_DIGIT(c)) {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }

    position += count;
    return value;
}

bool TomlDate::parseLocalTime(const std::string_view& sv) {
    size_t position = 0;

    // 解析时间部分 hh:mm:ss
    auto hour = ParseDigits(sv, position, 2);
    if (!hour || *hour > 23 || position >= sv.size() || sv[position] != ':') {
        return false;
    }
    position++;

    auto minute = ParseDigits(sv, position, 2);
    if (!minute || *minute > 59 || position >= sv.size() || sv[position] != ':') {
        return false;
    }
    position++;

    auto second = ParseDigits(sv, position, 2);
    if (!second || *second > 59) {
        return false;
    }

    // 存储时间部分
    setHour(*hour);
    setMinute(*minute);
    setSecond(*second);
    m_type = TomlDateTimeType::LOCAL_TIME;

    // 解析可选的亚秒部分
    if (position < sv.size() && sv[position] == '.') {
        position++;
        parseSubSecond(sv, position);
    }

    // 必须完全匹配整个字符串
    return position == sv.size();
}

bool TomlDate::parseDateTime(const std::string_view& sv) {
    size_t position = 0;
    // 解析日期部分 YYYY-MM-DD
    auto year = ParseDigits(sv, position, 4);
    if (!year || position >= sv.size() || sv[position] != '-') {
        return false;
    }
    position++;

    auto month = ParseDigits(sv, position, 2);
    if (!month || *month < 1 || *month > 12 || position >= sv.size() || sv[position] != '-') {
        return false;
    }
    position++;

    auto day = ParseDigits(sv, position, 2);
    if (!day || *day < 1 || *day > 31) {
        return false;
    }

    // 存储日期部分
    setYear(*year);
    setMonth(*month);
    setDay(*day);
    m_type = TomlDateTimeType::LOCAL_DATE;

    // 日期后无内容，解析成功
    if (position == sv.size()) {
        return true;
    }

    // 检查日期和时间之间的分隔符
    if (sv[position] != 'T' && sv[position] != 't' && sv[position] != ' ') {
        throw std::invalid_argument("Invalid separator after date in string: " + std::string(sv));
    }
    position++;

    // 解析时间部分
    return parseTimePart(sv, position);
}

void TomlDate::parse(const std::string_view& sv) {
    // 为确保重入安全，先清空所有可选成员
    this->reset();
    if (sv.empty()) {
        throw std::invalid_argument("Cannot parse an empty string.");
    }
    // 尝试按优先级解析不同格式
    if (parseDateTime(sv)) {
        return;
    }
    this->reset();
    if (parseLocalTime(sv)) {
        return;
    }
    // 所有解析尝试均失败
    this->reset();
    throw std::invalid_argument("String does not match any valid TOML date/time format: " +
                                std::string(sv));
}

std::string TomlDate::toString() const {
    std::ostringstream oss;
    oss << std::setfill('0');

    auto year = getYear();
    if (year.has_value()) {
        auto month = getMonth();
        auto day   = getDay();
        auto hour  = getHour();
        oss << std::setw(4) << *year << '-' << std::setw(2) << *month << '-' << std::setw(2)
            << *day;
        if (hour.has_value()) {
            oss << 'T';
        }
    }

    auto hour = getHour();
    if (hour.has_value()) {
        auto minute = getMinute();
        auto second = getSecond();
        oss << std::setw(2) << *hour << ':' << std::setw(2) << *minute << ':' << std::setw(2)
            << *second;
    }

    auto subSecond = getSubSecond();
    if (subSecond.has_value() && *subSecond > 0) {
        std::string subStr = std::to_string(*subSecond);
        subStr.insert(0, 9 - subStr.length(), '0');
        // TOML spec: 去掉尾部零
        subStr.erase(subStr.find_last_not_of('0') + 1, std::string::npos);
        oss << '.' << subStr;
    }

    auto tzOffset = getTzOffset();
    if (tzOffset.has_value()) {
        if (*tzOffset == 0) {
            oss << "000Z";
        } else {
            auto offsetVal = *tzOffset;
            // 输出符号
            oss << (offsetVal > 0 ? '+' : '-');
            // 计算时间
            offsetVal = std::abs(offsetVal);
            oss << std::setw(2) << offsetVal / 60 << ':' << std::setw(2) << offsetVal % 60;
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
    tm.tm_year  = *getYear() - 1900;
    tm.tm_mon   = *getMonth() - 1;
    tm.tm_mday  = *getDay();
    tm.tm_hour  = *getHour();
    tm.tm_min   = *getMinute();
    tm.tm_sec   = *getSecond();
    tm.tm_isdst = 0;  // Don't use daylight saving with UTC

    // timegm converts a struct tm (in UTC) to time_t
    std::time_t time = timegm(&tm);
    if (time == -1) {
        throw std::runtime_error("Failed to convert to time_t.");
    }

    auto timePoint = std::chrono::system_clock::from_time_t(time);

    // Add subseconds and subtract the offset to get the final UTC time_point
    auto subSecond = getSubSecond();
    if (subSecond.has_value()) {
        timePoint += std::chrono::nanoseconds(*subSecond);
    }

    timePoint -= std::chrono::minutes(*getTzOffset());

    return timePoint;
}

void TomlDate::reset() {
    m_type      = TomlDateTimeType::INVALID;
    m_core      = 0;
    m_subSecond = 0;
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
            case TomlType::String: m_value.string = new TomlString(*other.m_value.string); break;
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
        while (position < data.size() && (data[position] == ' ' || data[position] == '\t')) {      \
            position++; /* 移动到下一个字符 */                                             \
        }                                                                                          \
    } while (false)

/**
 * @brief 跳过空白字符和注释（直到行尾）
 * @param data 输入的字符串数据
 * @param position 当前读取位置（会被修改）
 *
 * @note 先跳过空白，再处理注释
 * @note 注释以#开始直到行尾（\n或\r）
 * @note 注释中禁止出现除制表符外的控制字符
 */
#define SKIP_WHITESPACE_AND_COMMENT(data, position)                                                \
    do {                                                                                           \
        while (position < data.size()) {                                                           \
            /* 1. 先跳过普通空白 */                                                         \
            SKIP_WHITESPACE(data, position);                                                       \
            /* 2. 检查是否遇到注释 */                                                      \
            if (position < data.size() && data[position] == '#') {                                 \
                /* 跳过注释内容，同时检查非法控制字符 */                          \
                position++; /* 跳过#符号 */                                                    \
                while (position < data.size()) {                                                   \
                    char c = data[position];                                                       \
                    /* 检查是否为行结束符 */                                              \
                    if (c == '\n' || c == '\r') {                                                  \
                        break;                                                                     \
                    }                                                                              \
                    /* 检查是否为非法控制字符（除制表符外） */                   \
                    if ((static_cast<unsigned char>(c) <= 0x1F && c != '\t') || c == 0x7F) {       \
                        throw TomlParseException(                                                  \
                            "Control character (except tab) not allowed in comment", position);    \
                    }                                                                              \
                    position++;                                                                    \
                }                                                                                  \
            } else {                                                                               \
                /* 非注释字符，结束跳过 */                                               \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (false)

/**
 * @brief 跳过行结束符（\\r\\n或者\\n）
 * @param data 输入的字符串数据（可以是std::string或std::string_view）
 * @param position 当前读取位置（会被修改）
 *
 * @note 先检查是否为\\r，若是则跳过；再检查是否为\\n，若是则跳过
 * @note 可处理\\r\\n、\\r、\\n等不同换行风格
 */
#define SKIP_CRLF(data, position)                                                                  \
    do {                                                                                           \
        if (position < data.size() && data[position] == '\r') {                                    \
            ++position;                                                                            \
        }                                                                                          \
        if (position < data.size() && data[position] == '\n') {                                    \
            ++position;                                                                            \
        }                                                                                          \
    } while (false)

/**
 * @brief 跳过所有无用字符（空白、注释、换行符）
 * @param data 输入的字符串数据
 * @param position 当前读取位置（会被修改）
 *
 * @note 依次执行：跳过空白 → 处理注释 → 处理换行符
 * @note 循环执行直到无法继续跳过字符（位置不再变化）
 * @note 注释以#开始直到行尾（\\n或\\r）
 */
#define SKIP_USELESS_CHAR(data, position)                                                          \
    do {                                                                                           \
        while (position < data.size()) {                                                           \
            /* 记录本次循环开始的位置 */                                                \
            size_t currentStart = position;                                                        \
            /* 跳过空白 */                                                                     \
            SKIP_WHITESPACE_AND_COMMENT(data, position);                                           \
            /* 处理换行 */                                                                     \
            SKIP_CRLF(data, position);                                                             \
            /* 如果本轮没有移动position，则退出循环 */                               \
            if (position <= currentStart) {                                                        \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (false)

static std::vector<std::pair<std::vector<std::string>, TomlValue>>
                 parseKeyValuePairs(std::string_view data, size_t& position);
static TomlValue parseBoolean(const std::string_view& data, size_t& position);
static std::vector<std::string>
parseTableHeader(const std::string_view& data, size_t& position, bool isArray);
static TomlString
parseBareKey(const std::string_view& data, size_t& position, bool isTable = false);
static TomlString parseQuotedKeys(const std::string_view& data, size_t& position);
static std::pair<std::vector<TomlString>, TomlValue> parseKeyValue(const std::string_view& data,
                                                                   size_t& position);
static TomlValue   parseValue(const std::string_view& data, size_t& position);
static TomlValue   parseNumber(const std::string_view& data, size_t& position);
static TomlValue   parseNumberOrDate(const std::string_view& data, size_t& position);
static TomlValue   parseArray(const std::string_view& data, size_t& position);
static TomlValue   parseObject(const std::string_view& data, size_t& position);
static TomlValue   parseString(const std::string_view& data, size_t& position);
static std::string parseUnicodeString(const std::string_view& data, size_t& position);
static std::string parseBasicString(const std::string_view& data, size_t& position);
static std::string parseMultiBasicString(const std::string_view& data, size_t& position);
static std::string parseLiteralString(const std::string_view& data, size_t& position);
static std::string parseMultiLiteralString(const std::string_view& data, size_t& position);
/**
 * @brief 快速检查字符串的某个位置是否“看起来像”一个TOML日期或时间的开始。
 *
 * 这是一个高性能的启发式检查，不进行完整验证。<br/>
 * 它只检查两种模式：<br/>
 * 1. "YYYY-" (4个数字后跟'-') -> 可能是日期或日期时间<br/>
 * 2. "hh:" (2个数字后跟':') -> 可能是时间
 *
 * @param sv 要检查的字符串视图。
 * @param position 开始检查的索引位置。
 * @return 如果看起来像日期或时间，则为 true，否则为 false。
 */
static bool LooksLikeDateTime(const std::string_view& sv, size_t position) noexcept;
/**
 * @brief 精确、高性能地验证一个字符串视图是否完全匹配TOML的四种日期时间格式之一。
 *
 * 此函数为自包含的单函数实现，严格遵循TOML v1.0.0规范，<br/>
 * 其中包括允许使用 'T', 't', 或空格作为日期和时间的分隔符。
 *
 * @param data 要验证的完整字符串视图。
 * @return 如果完全匹配，则为 true，否则为 false。
 */
static bool MatchFullDateTime(const std::string_view& data) noexcept;

static std::vector<std::pair<std::vector<std::string>, TomlValue>>
parseKeyValuePairs(std::string_view data, size_t& position) {
    std::vector<std::pair<std::vector<std::string>, TomlValue>> keyValues;

    while (position < data.size()) {
        SKIP_USELESS_CHAR(data, position);
        // 遇到新的table或者没有内容
        if (position >= data.size() || data[position] == '[') {
            break;
        }
        // 解析key-value
        keyValues.emplace_back(parseKeyValue(data, position));
    }

    return keyValues;
}

TomlValue parseBoolean(const std::string_view& data, size_t& position) {
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
parseTableHeader(const std::string_view& data, size_t& position, bool isArray) {
    // 当前字符一定为[,跳过表头
    position++;
    position += isArray;
    std::vector<std::string> headers;
    auto                     size = data.size();
    // 解析key
    while (position < size) {
        SKIP_WHITESPACE(data, position);
        if (position < size && (data[position] == '"' || data[position] == '\'')) {
            headers.emplace_back(parseQuotedKeys(data, position));
        } else {
            headers.emplace_back(parseBareKey(data, position, true));
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
    position++;
    // 如果是数组则还需要移动一次
    position += isArray;
    return headers;
}

TomlString parseBareKey(const std::string_view& data, size_t& position, bool isTable) {
    auto start = position;
    while (position < data.size()) {
        char c = data[position];
        if (std::isalnum(c) || c == '_' || c == '-') {
            position++;
        } else if (c == '=' || c == ' ' || c == '\t' || c == '.' || (isTable && c == ']')) {
            // 结束了
            if (position - start == 0) {
                throw TomlParseException("Invalid key", position);
            }
            return TomlString(data.substr(start, position - start));
        } else {
            break;
        }
    }
    throw TomlParseException("Unexpected content after Toml value", position);
}

TomlString parseQuotedKeys(const std::string_view& data, size_t& position) {
    if (data[position] == '"') {
        return parseBasicString(data, position);
    } else {
        return parseLiteralString(data, position);
    }
}

std::pair<std::vector<TomlString>, TomlValue> parseKeyValue(const std::string_view& data,
                                                            size_t&                 position) {
    // 当前data[position]一定有意义
    std::vector<TomlString> keys;
    auto                    size = data.size();
    // 解析key
    while (position < size) {
        SKIP_WHITESPACE(data, position);
        if (position < size && (data[position] == '"' || data[position] == '\'')) {
            keys.emplace_back(parseQuotedKeys(data, position));
        } else {
            keys.emplace_back(parseBareKey(data, position));
        }
        // 跳过空白字符
        SKIP_WHITESPACE(data, position);
        // 检查是否有点分隔符,有则继续解析
        if (position < size && data[position] == '.') {
            position++;
        } else {
            break;
        }
    }
    // 跳过空白，直到遇到=
    SKIP_WHITESPACE(data, position);
    if (position >= size || data[position] != '=') {
        throw TomlParseException("Expect = after a key", position);
    } else {
        position++;
    }
    // 解析value
    auto value = parseValue(data, position);
    SKIP_WHITESPACE_AND_COMMENT(data, position);
    // 换行
    if (position < size &&
        (data[position] == '\r' || data[position] == '\n' || data[position] == '}' ||
         data[position] == ']' || data[position] == ',')) {
        SKIP_CRLF(data, position);
    } else if (position < size) {
        throw TomlParseException("A line break is required after the value", position);
    }
    return {keys, value};
}

TomlValue parseValue(const std::string_view& data, size_t& position) {
    // 跳过前面的空白
    SKIP_WHITESPACE(data, position);
    // 根据当前字符判断是哪种类型
    char c = data[position];
    if (c == '"' || c == '\'') {
        return parseString(data, position);
    } else if (c == '+' || c == '-' || IS_DIGIT(c) || c == 'i' || c == 'n') {
        // 处理inf、nan、数字和日期
        return parseNumberOrDate(data, position);
    } else if (c == 't' || c == 'f') {
        return parseBoolean(data, position);
    } else if (c == '[') {
        // 内联表,不可修改
        return parseArray(data, position);
    } else if (c == '{') {
        // 内联表,不可修改
        return parseObject(data, position);
    } else {
        throw TomlParseException("invalid value", position);
    }
}

TomlValue parseNumber(const std::string_view& data, size_t& position) {
    size_t start = position;
    size_t size  = data.size();
    // 处理正负号
    bool isNegative = false;
    if (data[position] == '+' || data[position] == '-') {
        isNegative = (data[position] == '-');
        ++position;
        if (position < size && data[position] == '.') {
            throw TomlParseException(
                "Invalid numeric format: sign ('+'/'-') cannot be immediately followed by '.'",
                position);
        }
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
        // 只允许小写
        if (c == 'b') {
            base = 2;
            position += 2;
        } else if (c == 'o') {
            base = 8;
            position += 2;
        } else if (c == 'x') {
            base = 16;
            position += 2;
        }
    }
    // 只有十进制允许正负号
    if (base != 10 && (data[start] == '+' || data[start] == '-')) {
        throw TomlParseException("Sign ('+'/'-') is only allowed in decimal numbers", start);
    }

    // 判断是否浮点（需要推迟到后面判断）
    bool   isFloat = false;
    size_t scan    = position;
    // 在当前进制下判断是否数字是否合法
    auto isValidDigit = [base](unsigned char c) -> bool {
        switch (base) {
            case 2: return c == '0' || c == '1';
            case 8: return c >= '0' && c <= '7';
            case 10: return IS_DIGIT(c);
            case 16: return std::isxdigit(c);
            default: return false;
        }
    };
    // 检测前导0
    if (base == 10 && data[position] == '0' && position + 1 < size &&
        (IS_DIGIT(data[position + 1]) || data[position + 1] == '_')) {
        throw TomlParseException("Leading zeros are not allowed", position);
    }
    // 读取主数字部分（整数或浮点）
    while (scan < size && (isValidDigit(data[scan]) || data[scan] == '_')) {
        if (data[scan] != '_') {
            // 是数字
            ++scan;
        } else {
            // 向后多look一个字符,因为_后必须跟随数字
            if (scan + 1 < size && isValidDigit(data[scan + 1])) {
                ++scan;
            } else {
                throw TomlParseException(
                    "Invalid numeric format: underscore '_' must be followed by a digit", start);
            }
        }
    }
    // 浮点检测（仅 base-10 支持）
    if (base == 10 && scan < size && data[scan] == '.') {
        // 跳过.
        isFloat = true;
        ++scan;
        // 不允许._
        if (scan < size && data[scan] == '_') {
            throw TomlParseException(
                "Invalid numeric format: '.' cannot be immediately followed by underscore '_'",
                position);
        }
        bool hasFraction = false;
        while (scan < size && (IS_DIGIT(data[scan]) || data[scan] == '_')) {
            if (data[scan] != '_') {
                // 是数字
                hasFraction = true;
                ++scan;
            } else {
                // 向后多look一个字符,因为_后必须跟随数字
                if (scan + 1 < size && IS_DIGIT(data[scan + 1])) {
                    ++scan;
                } else {
                    throw TomlParseException(
                        "Invalid numeric format: underscore '_' must be followed by a digit",
                        start);
                }
            }
        }
        if (!hasFraction) {
            throw TomlParseException("Expected digits after '.'", scan);
        }
    }
    // 指数部分（仅 base-10 支持）
    if (base == 10 && scan < size && (data[scan] == 'e' || data[scan] == 'E')) {
        isFloat = true;
        // 跳过e
        ++scan;
        // 跳过符号
        if (scan < size && (data[scan] == '+' || data[scan] == '-')) {
            ++scan;
        }
        // 如果直接跟着_则报错(因为整数第一个不可能为_)
        if (scan < size && data[scan] == '_') {
            throw TomlParseException("Expected digits after exponent, instead of a _", scan);
        }
        bool hasExpDigit = false;
        while (scan < size && (IS_DIGIT(data[scan]) || data[scan] == '_')) {
            if (data[scan] != '_') {
                // 是数字
                hasExpDigit = true;
                ++scan;
            } else {
                // 向后多look一个字符,因为_后必须跟随数字
                if (scan + 1 < size && IS_DIGIT(data[scan + 1])) {
                    ++scan;
                } else {
                    throw TomlParseException(
                        "Invalid numeric format: underscore '_' must be followed by a digit",
                        start);
                }
            }
        }
        if (!hasExpDigit) {
            throw TomlParseException("Expected digits after exponent", scan);
        }
    }
    // 数字部分
    std::string raw(data.substr(position, scan - position));
    if (isNegative) {
        raw.insert(0, 1, '-');
    }
    position = scan;

    // 检查下划线合法性
    if (raw.front() == '_' || raw.back() == '_') {
        throw TomlParseException("Underscore at start or end", start);
    }
    // 去除下划线
    raw.erase(std::remove(raw.begin(), raw.end(), '_'), raw.end());

    if (isFloat) {
        double result  = 0;
        auto [ptr, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), result);
        if (ec != std::errc() || ptr != raw.data() + raw.size()) {
            throw TomlParseException("Invalid float", start);
        }
        return result;
    } else {
        int64_t result = 0;
        auto [ptr, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), result, base);
        if (ec != std::errc() || ptr != raw.data() + raw.size()) {
            throw TomlParseException("Invalid integer", start);
        }
        return result;
    }
}

TomlValue parseNumberOrDate(const std::string_view& data, size_t& position) {
    size_t original = position;
    // 判断是否为日期
    if (LooksLikeDateTime(data, position)) {
        // 完整匹配
        size_t end         = position;
        auto   isValidChar = [](char c) -> bool {
            return std::isalnum(c) || c == '-' || c == ':' || c == 'T' || c == 'Z' || c == '+' ||
                   c == '.';
        };
        while (end < data.size() &&
               (isValidChar(data[end]) ||
                (data[end] == ' ' && end + 1 < data.size() && isValidChar(data[end + 1])))) {
            ++end;
        }
        // 可能的日期字符串
        auto maybeDate = data.substr(position, end - position);
        // 判断是否为日期
        if (MatchFullDateTime(maybeDate)) {
            position = end;
            return TomlDate(maybeDate);
        }
    }
    // 回退并作为普通数值解析
    position = original;
    return parseNumber(data, position);
}

#define SKIP_ALL(data, position)                                                                   \
    do {                                                                                           \
        while (position < data.size()) {                                                           \
            /* 跳过空格、制表符 */                                                         \
            SKIP_WHITESPACE(data, position);                                                       \
            /* 注释：跳过整行直到换行符 */                                             \
            SKIP_WHITESPACE_AND_COMMENT(data, position);                                           \
            /* 跳过换行符（\r\n 或 \n）*/                                                  \
            SKIP_CRLF(data, position);                                                             \
            /* 如果都不是，就 break */                                                      \
            if (position < data.size() && data[position] != '#' && data[position] != ' ' &&        \
                data[position] != '\t' && data[position] != '\r' && data[position] != '\n') {      \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (false)

TomlValue parseArray(const std::string_view& data, size_t& position) {
    // 当前字符一定为[
    position++;
    SKIP_ALL(data, position);
    // 解析以,分割的一个个value
    TomlArray array;
    bool      expectReturn = false;
    while (position < data.size()) {
        SKIP_ALL(data, position);
        if (data[position] == ']') {
            position++;
            return array;
        }
        array.emplace_back(parseValue(data, position));
        SKIP_ALL(data, position);
        // 如果为,则说明还有值,否则则应该return了
        if (position < data.size() && data[position] == ',') {
            position++;
        }
    }
    throw TomlParseException("Unclosed array: missing ']'", position);
}

TomlValue parseObject(const std::string_view& data, size_t& position) {
    // 当前一定为{ (object不同于array,不允许换行)
    position++;
    TomlValue object;
    bool      needValue = false;
    while (position < data.size()) {
        SKIP_WHITESPACE_AND_COMMENT(data, position);
        if (data[position] == '}') {
            // 在内联表中的最后一个键/值对之后，不允许终止逗号
            if (!needValue) {
                position++;
                return object;
            } else {
                break;
            }
        }
        // 解析一个个key-value
        auto node    = &object;
        auto [ks, v] = parseKeyValue(data, position);
        for (size_t i = 0; !ks.empty() && i < ks.size() - 1; i++) {
            if (node->isObject()) {
                node = &(*node)[ks[i]];
            } else {
                throw TomlParseException("Cannot create nested key: parent is not an object",
                                         position);
            }
        }
        if (node->isObject()) {
            node->set(ks.back(), v);
        } else {
            throw TomlParseException("Cannot set value: target is not an object", position);
        }
        SKIP_WHITESPACE_AND_COMMENT(data, position);
        if (position < data.size() && data[position] == ',') {
            needValue = true;
            position++;
        } else {
            needValue = false;
        }
    }
    throw TomlParseException("Unclosed object: missing '}'", position);
}

#undef SKIP_ALL

#define MATCH3(sv, pos, ch)                                                                        \
    ((pos) + 2 < (sv).size() && (sv)[pos] == (ch) && (sv)[(pos) + 1] == (ch) &&                    \
     (sv)[(pos) + 2] == (ch))

TomlValue parseString(const std::string_view& data, size_t& position) {
    // 判断是哪种类型
    char c = data[position];
    if (MATCH3(data, position, c)) {
        // " 基本字符串
        // """ 多行基本字符串
        // ' 字面字符串
        // ''' 多行字面字符串
        if (c == '"') {
            return parseMultiBasicString(data, position);
        } else if (c == '\'') {
            return parseMultiLiteralString(data, position);
        } else {
            throw TomlParseException("not a string", position);
        }
    } else {
        if (c == '"') {
            return parseBasicString(data, position);
        } else if (c == '\'') {
            return parseLiteralString(data, position);
        } else {
            throw TomlParseException("not a string", position);
        }
    }
}

std::string parseUnicodeString(const std::string_view& data, size_t& position) {
    auto parseHex = [&](int length) {
        if (position + length >= data.size()) {
            throw TomlParseException("Unexpected end in Unicode escape", position);
        }
        // 获取字符串
        auto hex = data.substr(position, length);
        position += length;
        int32_t codePoint;
        auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), codePoint, 16);
        if (ec != std::errc() || ptr != hex.data() + hex.size()) {
            throw TomlParseException("Invalid hexadecimal string " + std::string(hex), position);
        }
        return codePoint;
    };
    // 获取unicode位数
    auto hexLength = data[position++] == 'u' ? 4 : 8;
    // 解析出码点
    auto codePoint = parseHex(hexLength);
    // 校验码点是否为合法的 Unicode 标量值, toml不允许代理对
    if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        // 处理非法码点
        throw TomlParseException(
            "Invalid Unicode code point: " +
                std::string(data.substr(position - hexLength - 2, hexLength + 2)),
            position);
    }
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

std::string parseBasicString(const std::string_view& data, size_t& position) {
    // 跳过"
    ++position;

    std::string result;
    while (position < data.size()) {
        char c = data[position];
        // 结束符
        if (c == '"') {
            ++position;
            return result;
        }
        // 任何 Unicode
        // 字符都可以使用，除了那些必须转义的：引号，反斜杠，以及除制表符外的控制字符（U+0000 至
        // U+0008，U+000A 至 U+001F，U+007F）。
        if (((0x0000 <= c && c <= 0x0008) || (0x000A <= c && c <= 0X001F) || c == 0x007F) &&
            c != 0x0009) {
            throw TomlParseException(
                "Any Unicode character can be used, except for those that must be escaped: "
                "quotation marks, backslashes, and control characters other than tabs ",
                position);
        }
        // 转义
        if (c == '\\') {
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
                    // 回退为\u
                    --position;
                    result += parseUnicodeString(data, position);
                    break;
                default:
                    throw TomlParseException("Unknown escape: \\" + std::string(1, esc), position);
            }
        } else {
            if (c == '\r' || c == '\n') {
                throw TomlParseException("Line wrapping is not allowed in Basic String", position);
            }
            result += c;
            ++position;
        }
    }
    throw TomlParseException("Unterminated basic string", position);
}

std::string parseMultiBasicString(const std::string_view& data, size_t& position) {
    // 此时当前字符串一定为"""
    position += 3;
    // 跳过开头的换行符（如果存在）
    SKIP_CRLF(data, position);
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
                if (position + 5 < data.size() && data[position + 5] == '"') {
                    throw TomlParseException("not allow 3 \" in multi-basic string", position);
                }
            } else {
                position += 3;
                return result;
            }
        }
        char c = data[position++];
        if (c == '\\' && !inEscape) {
            // 1. 先判断该行换行前是否存在正常字符
            auto cp          = position;
            bool containChar = false;
            while (cp < data.size() && (data[cp] != '\n' && data[cp] != '\r')) {
                if (data[cp] == ' ' || data[cp] == '\t') {
                    cp++;
                } else {
                    containChar = true;
                    break;
                }
            }
            if (containChar) {
                inEscape = true;
            } else {
                // 2. 不存在字符才可能为\换行
                if (position < data.size() && (data[position] == '\r' || data[position] == '\n' ||
                                               data[position] == ' ' || data[position] == '\t')) {
                    // 遇到了toml语法中的\换行
                    // 当一行的最后一个非空白字符是未被转义的
                    // \ 时，它会连同它后面的所有空白（包括换行）一起被去除，直到下一个非空白字符或结束引号为止。
                    // 处理\后面的空白
                    while (position < data.size() &&
                           (data[position] == '\r' || data[position] == '\n' ||
                            data[position] == ' ' || data[position] == '\t')) {
                        position++;
                    }
                } else {
                    inEscape = true;  // 普通转义字符，下一个字符特殊处理
                }
            }
        } else if ((0x0000 <= c && c <= 0x0008) || c == 0x000B || c == 0x000C ||
                   (0x000E <= c && c <= 0x001F) || c == 0x007F) {
            throw TomlParseException(
                "Any Unicode character can be used, except for those that must be escaped: "
                "backslashes and control characters except tabs, line breaks, and carriage returns",
                position);
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
                    result += parseUnicodeString(data, position);
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

std::string parseLiteralString(const std::string_view& data, size_t& position) {
    // 跳过开头的'
    ++position;
    std::string result;
    while (position < data.size()) {
        char c = data[position];

        if (c == '\'') {
            ++position;
            return result;
        } else if (c == '\r' || c == '\n') {
            throw TomlParseException("Line wrapping is not allowed in Basic String", position);
        } else if (((0x0000 <= c && c <= 0x001F) || (c == 0x007F)) && c != 0x0009) {
            throw TomlParseException(
                "All control characters other than tabs are not allowed in literal strings",
                position);
        }
        result += c;
        ++position;
    }

    throw TomlParseException("Unterminated literal string", position);
}

std::string parseMultiLiteralString(const std::string_view& data, size_t& position) {
    position += 3;
    // 跳过开头的换行符（如果存在）
    SKIP_CRLF(data, position);
    std::string result;
    result.reserve(32);
    while (position < data.size()) {
        char c = data[position];
        if (MATCH3(data, position, '\'')) {
            // 已经遇到了三个引号,但却不一定结束
            // 说明后面还有",当前data[position]其实可能被视为普通字符,所允许的最大也不过为2+3
            if (position + 3 < data.size() && data[position + 3] == '\'') {
                // 将当前两个字符视为普通字符即可
                if (position + 5 < data.size() && data[position + 5] == '\'') {
                    throw TomlParseException("not allow 3 ' in multi-literal string", position);
                }
            } else {
                // 结束了
                position += 3;
                return result;
            }
        } else if (((0x0000 <= c && c <= 0x001F) || (c == 0x007F)) && c != 0x0009 && c != '\r' &&
                   c != '\n') {
            throw TomlParseException(
                "All control characters other than tabs are not allowed in multi-literal strings",
                position);
        }
        result += c;
        position++;
    }
    throw TomlParseException("Unterminated multi-line literal string", position);
}

#undef MATCH3

bool LooksLikeDateTime(const std::string_view& sv, size_t position) noexcept {
    // 检查 "YYYY-" 模式
    if (position + 5 <= sv.length()) {
        if (IS_DIGIT(sv[position]) && IS_DIGIT(sv[position + 1]) && IS_DIGIT(sv[position + 2]) &&
            IS_DIGIT(sv[position + 3]) && sv[position + 4] == '-') {
            return true;
        }
    }

    // 检查 "hh:" 模式
    if (position + 3 <= sv.length()) {
        if (IS_DIGIT(sv[position]) && IS_DIGIT(sv[position + 1]) && sv[position + 2] == ':') {
            return true;
        }
    }

    return false;
}

bool MatchFullDateTime(const std::string_view& data) noexcept {
    if (data.empty()) {
        return false;
    }

    size_t       pos = 0;
    const size_t len = data.length();

    // -----------------------------------------------------------------
    // 分支 1: 尝试解析以日期 (YYYY-MM-DD) 开头的格式
    // -----------------------------------------------------------------
    if (len >= 10 && (data[0] >= '0' && data[0] <= '9') && (data[1] >= '0' && data[1] <= '9') &&
        (data[2] >= '0' && data[2] <= '9') && (data[3] >= '0' && data[3] <= '9') &&
        (data[4] == '-') && (data[5] >= '0' && data[5] <= '9') &&
        (data[6] >= '0' && data[6] <= '9') && (data[7] == '-') &&
        (data[8] >= '0' && data[8] <= '9') && (data[9] >= '0' && data[9] <= '9')) {
        // 检查月份和日期的数值范围
        const int month = (data[5] - '0') * 10 + (data[6] - '0');
        const int day   = (data[8] - '0') * 10 + (data[9] - '0');

        if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
            // ---- 日期部分有效 ----
            pos = 10;

            // 如果字符串到此结束, 则是合法的 Local Date
            if (pos == len) {
                return true;
            }

            // 如果后面有内容, 分隔符必须是 'T', 't', 或空格
            if (data[pos] != 'T' && data[pos] != 't' && data[pos] != ' ') {
                return false;
            }
            pos++;  // 消耗分隔符

            // ---- 解析时间部分 (hh:mm:ss) ----
            if (pos + 8 > len)
                return false;
            const int hour   = (data[pos] - '0') * 10 + (data[pos + 1] - '0');
            const int minute = (data[pos + 3] - '0') * 10 + (data[pos + 4] - '0');
            const int second = (data[pos + 6] - '0') * 10 + (data[pos + 7] - '0');

            if (!((data[pos] >= '0' && data[pos] <= '9') &&
                  (data[pos + 1] >= '0' && data[pos + 1] <= '9') && (data[pos + 2] == ':') &&
                  (data[pos + 3] >= '0' && data[pos + 3] <= '9') &&
                  (data[pos + 4] >= '0' && data[pos + 4] <= '9') && (data[pos + 5] == ':') &&
                  (data[pos + 6] >= '0' && data[pos + 6] <= '9') &&
                  (data[pos + 7] >= '0' && data[pos + 7] <= '9') &&
                  (hour <= 23 && minute <= 59 && second <= 59))) {
                return false;
            }
            pos += 8;

            // ---- 解析可选的亚秒部分 (.ffffff) ----
            if (pos < len && data[pos] == '.') {
                pos++;
                const size_t frac_start = pos;
                while (pos < len && (data[pos] >= '0' && data[pos] <= '9')) {
                    pos++;
                }
                if (pos == frac_start)
                    return false;
            }

            // ---- 解析可选的时区偏移部分 ----
            if (pos < len) {
                const char offset_char = data[pos];
                if (offset_char == 'Z' || offset_char == 'z') {
                    pos++;
                } else if (offset_char == '+' || offset_char == '-') {
                    pos++;
                    if (pos + 5 > len)
                        return false;
                    const int off_hour = (data[pos] - '0') * 10 + (data[pos + 1] - '0');
                    const int off_min  = (data[pos + 3] - '0') * 10 + (data[pos + 4] - '0');

                    if (!((data[pos] >= '0' && data[pos] <= '9') &&
                          (data[pos + 1] >= '0' && data[pos + 1] <= '9') &&
                          (data[pos + 2] == ':') &&
                          (data[pos + 3] >= '0' && data[pos + 3] <= '9') &&
                          (data[pos + 4] >= '0' && data[pos + 4] <= '9') &&
                          (off_hour <= 23 && off_min <= 59))) {
                        return false;
                    }
                    pos += 5;
                }
            }

            // 最终检查: 必须消耗掉整个字符串
            return pos == len;
        }
    }

    // -----------------------------------------------------------------
    // 分支 2: 尝试解析纯时间格式 (hh:mm:ss)
    // -----------------------------------------------------------------
    pos = 0;
    if (len >= 8 && (data[pos + 2] == ':') && (data[pos + 5] == ':') &&
        (data[pos] >= '0' && data[pos] <= '9') && (data[pos + 1] >= '0' && data[pos + 1] <= '9') &&
        (data[pos + 3] >= '0' && data[pos + 3] <= '9') &&
        (data[pos + 4] >= '0' && data[pos + 4] <= '9') &&
        (data[pos + 6] >= '0' && data[pos + 6] <= '9') &&
        (data[pos + 7] >= '0' && data[pos + 7] <= '9')) {
        const int hour   = (data[pos] - '0') * 10 + (data[pos + 1] - '0');
        const int minute = (data[pos + 3] - '0') * 10 + (data[pos + 4] - '0');
        const int second = (data[pos + 6] - '0') * 10 + (data[pos + 7] - '0');

        if (hour <= 23 && minute <= 59 && second <= 59) {
            pos += 8;

            if (pos < len && data[pos] == '.') {
                pos++;
                const size_t frac_start = pos;
                while (pos < len && (data[pos] >= '0' && data[pos] <= '9')) {
                    pos++;
                }
                if (pos == frac_start)
                    return false;
            }

            return pos == len;
        }
    }

    return false;
}

namespace parser {
    TomlValue parse(std::string_view data) {
        size_t    position = 0;
        TomlValue root;
        // 按行解析
        size_t size = data.size();
        // 1. 先解析顶层内容
        if (position < size && data[position] != '[') {
            // 解析顶层属性(key-value)
            auto keyValues = parseKeyValuePairs(data, position);
            // 根据表头添加数据
            // 对于当前节点赋值
            for (const auto& [k, v] : keyValues) {
                TomlValue* node = &root;
                for (size_t i = 0; !k.empty() && i < k.size() - 1; i++) {
                    if (node->isObject()) {
                        node = &(*node)[k[i]];
                    } else {
                        throw TomlException("node is not a object");
                    }
                }
                if (node->isObject()) {
                    if (node->asObject().find(k.back()) == node->asObject().end()) {
                        node->set(k.back(), v);
                    } else {
                        throw TomlParseException("key '" + k.back() + "' has existed", position);
                    }
                } else {
                    throw TomlParseException("not a object", position);
                }
            }
        }
        // 2. 解析[]中的内容
        while (position < size) {
            // 跳过无用字符串
            SKIP_WHITESPACE_AND_COMMENT(data, position);
            if (position >= size) {
                break;
            }
            if (data[position] != '[') {
                throw TomlParseException("Expected table header", position);
            }
            // 解析表头
            bool isArray = false;
            if (position + 1 < size && data[position + 1] == '[') {
                isArray = true;
            }
            // 解析表头（然后期待换行）
            auto headers = parseTableHeader(data, position, isArray);
            // 表头后可能存在空白和注释
            SKIP_WHITESPACE_AND_COMMENT(data, position);
            // 表头需要换货
            if (position < size && data[position] != '\r' && data[position] != '\n') {
                throw TomlParseException("A line break is required after the value", position);
            }
            SKIP_CRLF(data, position);

            // 解析下面的key-value
            auto keyValues = parseKeyValuePairs(data, position);
            // key-values解析完毕, 根据表头添加数据
            // 查找要添加的表节点
            TomlValue* node = &root;
            for (size_t i = 0; !headers.empty() && i < headers.size() - 1; i++) {
                if (node->isObject()) {
                    node = &(*node)[headers[i]];
                } else if (node->isArray()) {
                    auto& array = node->asArray();
                    // 获取node的一个back
                    if (array.empty()) {
                        // 创建一个object推入node中
                        array.emplace_back(TomlObject{{headers[i], TomlValue()}});
                    } else {
                        if (auto it = std::find_if(
                                array.rbegin(), array.rend(),
                                [](const auto& element) { return element.isObject(); });
                            it != array.rend()) {
                            // 找到了对象元素
                            if (it->asObject().find(headers[i]) == it->asObject().end()) {
                                it->set(headers[i], isArray ? TomlArray() : TomlValue());
                            }
                        } else {
                            // 没有找到对象元素, 向array中放入一个空的{}
                            array.emplace_back(TomlObject{{headers[i], TomlValue()}});
                        }
                    }
                    node = &(node->asArray().back()[headers[i]]);
                } else {
                    throw TomlParseException("node should be a array or object", position);
                }
            }
            // 是array,当前node又是一个object
            // 相当于[[ a.b ]]的 a 为 node, b是headers.back()
            if (isArray && node->isObject() &&
                node->asObject().find(headers.back()) == node->asObject().end()) {
                node->set(headers.back(), TomlArray());
            }
            // 如果不存在则会创建一个object
            if (node->isObject()) {
                node = &(*node)[headers.back()];
            } else if (node->isArray()) {
                // 获取node的一个back
                auto& array = node->asArray();
                if (array.empty()) {
                    // 创建一个object推入node中
                    array.emplace_back(TomlObject{{headers.back(), TomlValue()}});
                } else {
                    // 反向遍历是为了找到[该数组里最近定义的表元素]
                    if (auto it =
                            std::find_if(array.rbegin(), array.rend(),
                                         [](const auto& element) { return element.isObject(); });
                        it != array.rend()) {
                        // 找到了对象元素
                        if (it->asObject().find(headers.back()) == it->asObject().end()) {
                            it->set(headers.back(), isArray ? TomlArray() : TomlValue());
                        }
                    } else {
                        // 没有找到对象元素, 向array中放入一个空的{}
                        array.emplace_back(TomlObject{{headers.back(), TomlValue()}});
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
                        for (size_t i = 0; !ks.empty() && i < ks.size() - 1; i++) {
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
                    for (size_t i = 0; !ks.empty() && i < ks.size() - 1; i++) {
                        tempNode = &(*tempNode)[ks[i]];
                    }
                    tempNode->set(ks.back(), v);
                }
            }
        }
        // 跳过无用字符串
        SKIP_WHITESPACE_AND_COMMENT(data, position);
        if (position != data.size()) {
            throw TomlParseException("Unexpected content after Toml value", position);
        }
        return root;
    }
}  // namespace parser

#undef SKIP_USELESS_CHAR
#undef SKIP_CRLF
#undef SKIP_WHITESPACE_AND_COMMENT
#undef SKIP_WHITESPACE

static void        stringifyValue(const TomlValue& value, std::ostringstream& oss);
inline static void stringifyBoolean(const TomlValue& value, std::ostringstream& oss);
inline static void stringifyInteger(const TomlValue& value, std::ostringstream& oss);
static void        stringifyDouble(const TomlValue& value, std::ostringstream& oss);
static bool        stringIsBareKey(const std::string& s);
inline static void stringifyString(const TomlValue& value, std::ostringstream& oss);
static void        stringifyString(const std::string& s, std::ostringstream& oss);
inline static void stringifyDate(const TomlValue& value, std::ostringstream& oss);
static void        stringifyArray(const TomlValue& value, std::ostringstream& oss);
static void        stringifyInlineObject(const TomlObject& obj, std::ostringstream& oss);
static bool        isArrayOfTables(const TomlValue& value);
inline static void stringifyObject(const TomlValue& value, std::ostringstream& oss);
static void
stringifyObject(const TomlObject& object, std::ostringstream& oss, const std::string& prefix);

void stringifyValue(const TomlValue& value, std::ostringstream& oss) {
    switch (value.type()) {
        case TomlType::Boolean: return stringifyBoolean(value, oss);
        case TomlType::Integer: return stringifyInteger(value, oss);
        case TomlType::Double: return stringifyDouble(value, oss);
        case TomlType::String: return stringifyString(value, oss);
        case TomlType::Date: return stringifyDate(value, oss);
        case TomlType::Array: return stringifyArray(value, oss);
        case TomlType::Object: return stringifyObject(value, oss);
    }
}

inline void stringifyBoolean(const TomlValue& value, std::ostringstream& oss) {
    oss << (value.get<bool>() ? "true" : "false");
}

inline void stringifyInteger(const TomlValue& value, std::ostringstream& oss) {
    oss << value.get<int64_t>();
}

void stringifyDouble(const TomlValue& value, std::ostringstream& oss) {
    auto num = value.get<double>();
    if (std::isinf(num)) {
        if (std::signbit(num)) {
            oss << "-inf";
        } else {
            oss << "inf";
        }
    } else if (std::isnan(num)) {
        oss << "nan";
    } else {
        oss << std::setprecision(std::numeric_limits<double>::max_digits10) << num;
    }
}

bool stringIsBareKey(const std::string& s) {
    // 空字符串不是合法的bareKey
    if (s.empty()) {
        return false;
    }
    // 第一个字符不能是数字
    if (IS_DIGIT(s.front())) {
        return false;
    }
    // 使用std::all_of检查剩余字符是否符合要求
    return std::all_of(s.begin() + 1, s.end(),
                       [](char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}

inline void stringifyString(const TomlValue& value, std::ostringstream& oss) {
    stringifyString(value.get<std::string>(), oss);
}

void stringifyString(const std::string& s, std::ostringstream& oss) {
    oss << '"';
    for (char c : s) {
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
            oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(uc);
            continue;
        }

        // 非控制字符直接输出
        oss << c;
    }
    oss << '"';
}

inline void stringifyDate(const TomlValue& value, std::ostringstream& oss) {
    oss << value.asDate();
}

void stringifyArray(const TomlValue& value, std::ostringstream& oss) {
    const auto& array = value.get<TomlArray>();
    oss << "[";
    for (size_t i = 0; i < array.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        const auto& item = array[i];

        if (item.type() == TomlType::Object) {
            stringifyInlineObject(item.get<TomlObject>(), oss);
        } else {
            stringifyValue(item, oss);
        }
    }
    oss << "]";
}

void stringifyInlineObject(const TomlObject& obj, std::ostringstream& oss) {
    oss << "{ ";
    bool first = true;
    for (const auto& [key, val] : obj) {
        if (!first) {
            oss << ", ";
        }
        first = false;
        // 输出key和value
        oss << '"' << key << "\" = ";
        stringifyValue(val, oss);
    }
    oss << " }";
}

bool isArrayOfTables(const TomlValue& value) {
    if (value.type() != TomlType::Array) {
        return false;
    }
    const auto& array = value.asArray();
    if (array.empty()) {
        return false;
    }
    return std::all_of(array.begin(), array.end(),
                       [](const TomlValue& item) { return item.type() == TomlType::Object; });
}

inline void stringifyObject(const TomlValue& value, std::ostringstream& oss) {
    stringifyObject(value.asObject(), oss, "");
}

void stringifyObject(const TomlObject& object, std::ostringstream& oss, const std::string& prefix) {
    // 🔹 1. 输出非 Object 的字段（顶层属性）
    for (const auto& [key, val] : object) {
        if (val.type() != TomlType::Object && !isArrayOfTables(val)) {
            if (stringIsBareKey(key)) {
                stringifyString(key, oss);
                oss << " = ";
            } else {
                oss << '"';
                stringifyString(key, oss);
                oss << "\" = ";
            }
            stringifyValue(val, oss);
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
            stringifyObject(child, oss, fullKey);
        } else if (isArrayOfTables(val)) {
            const auto& array = val.asArray();
            std::string fullKey;
            if (prefix.empty()) {
                fullKey = key;
            } else {
                fullKey.append(prefix).append(".").append(key);
            }
            for (const auto& elem : array) {
                oss << "\n[[" << fullKey << "]]\n";
                stringifyObject(elem.asObject(), oss, fullKey);
            }
        }
    }
}

namespace parser {
    std::string stringify(const TomlValue& value) {
        std::ostringstream oss;
        stringifyValue(value, oss);
        return oss.str();
    }
}  // namespace parser

#undef IS_DIGIT
}  // namespace cctoml
#pragma clang diagnostic pop