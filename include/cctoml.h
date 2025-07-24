#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "google-explicit-constructor"

#ifndef CCTOML_TOML_H
#    define CCTOML_TOML_H

#    include <chrono>
#    include <cstdint>
#    include <map>
#    include <optional>
#    include <stdexcept>
#    include <string>
#    include <unordered_map>
#    include <vector>

namespace cctoml {

/**
 * @class TomlException
 * @brief TOML 相关操作的异常基类。
 *
 * 继承自 std::runtime_error，用于处理 TOML 解析和操作中的通用错误。
 */
class TomlException : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/**
 * @class TomlParseException
 * @brief TOML 解析过程中的异常类。
 *
 * 用于报告 TOML 数据解析时发生的具体错误，包含错误发生的位置信息。
 */
class TomlParseException : public TomlException {
  public:
    /**
     * @brief 构造函数，创建包含错误信息和位置的异常。
     * @param data 错误描述信息。
     * @param position 错误发生的位置（字符索引）
     */
    TomlParseException(const std::string& data, size_t position)
        : TomlException(data + ", position: " + std::to_string(position)) {}
};

/**
 * @enum TomlType
 * @brief 表示 TOML 数据类型的枚举。
 *
 * 定义了 TOML 支持的所有数据类型。
 */
enum class TomlType {
    Boolean,  ///< 布尔值
    Integer,  ///< 整型
    Double,   ///< 浮点型
    String,   ///< 字符串
    Date,     ///< 日期
    Array,    ///< 数组
    Object    ///< 对象
};

class TomlValue;

/**
 * @namespace cctoml::parser
 * @brief TOML 解析和序列化的工具函数命名空间。
 */
namespace parser {
    /**
     * @brief 解析 TOML 格式的字符串数据。
     * @param data 输入的 TOML 数据（字符串视图）
     * @return 解析结果，返回一个 TomlValue 对象。
     * @throws TomlParseException 如果解析失败，抛出包含错误信息的异常。
     */
    TomlValue parse(std::string_view data);

    /**
     * @enum StringifyType
     * @brief 序列化格式的枚举。
     *
     * 定义了支持的序列化输出格式。
     */
    enum StringifyType {
        TO_TOML,  ///< toml字符串
        TO_JSON,  ///< json字符串
        TO_YAML   ///< yaml字符串
    };

    /**
     * @brief 将 TomlValue 序列化为指定格式的字符串。
     * @param value 要序列化的 TomlValue 对象。
     * @param type 序列化格式（TO_TOML、TO_JSON 或 TO_YAML）
     * @param indent 缩进空格数（仅对 JSON 和 YAML 有效，0 表示无缩进）
     * @return 序列化后的字符串。
     */
    std::string
    stringify(const TomlValue& value, StringifyType type = StringifyType::TO_TOML, int indent = 0);
}  // namespace parser

using TomlString = std::string;                      ///< TOML 字符串类型别名。
using TomlArray  = std::vector<TomlValue>;           ///< TOML 数组类型别名。
using TomlObject = std::map<TomlString, TomlValue>;  ///< TOML 对象类型别名（键值对映射）

/**
 * @class TomlDate
 * @brief 表示 TOML 格式的日期和时间。
 *
 * 支持 TOML 规范中的四种日期/时间格式：偏移日期时间、本地日期时间、本地日期和本地时间。
 * 提供解析、存储和序列化日期时间的功能。
 */
class TomlDate {
  public:
    /**
     * @enum TomlDateTimeType
     * @brief 表示 TOML 日期/时间类型的枚举。
     */
    enum class TomlDateTimeType {
        INVALID,           ///< 无效日期/时间
        OFFSET_DATE_TIME,  ///< 带时区偏移的日期时间（如 2025-07-22T15:00:00Z）
        LOCAL_DATE_TIME,   ///< 本地日期时间（如 2025-07-22T15:00:00）
        LOCAL_DATE,        ///< 本地日期（如 2025-07-22）
        LOCAL_TIME         ///< 本地时间（如 15:00:00）
    };

  public:
    /**
     * @brief 从 TOML 格式的字符串构造 TomlDate 对象。
     * @param s 输入的 TOML 日期/时间字符串。
     * @throws TomlException 如果字符串格式无效，抛出异常。
     */
    TomlDate(std::string_view s) : m_type(TomlDateTimeType::INVALID) {
        parse(s);
    }

    /**
     * @brief 拷贝构造函数。
     * @param date 要拷贝的 TomlDate 对象。
     */
    TomlDate(const TomlDate& date) noexcept;

    /**
     * @brief 移动构造函数。
     * @param date 要移动的 TomlDate 对象。
     */
    TomlDate(TomlDate&& date) noexcept;

    /**
     * @brief 拷贝赋值运算符。
     * @param date 要拷贝的 TomlDate 对象。
     * @return 自身引用。
     */
    TomlDate& operator=(const TomlDate& date) noexcept;

    /**
     * @brief 移动赋值运算符。
     * @param date 要移动的 TomlDate 对象。
     * @return 自身引用。
     */
    TomlDate& operator=(TomlDate&& date) noexcept;

    /**
     * @brief 获取日期/时间的类型。
     * @return 当前对象的 TomlDateTimeType。
     */
    inline TomlDateTimeType type() const noexcept {
        return m_type;
    }

    /**
     * @brief 检查是否为偏移日期时间类型。
     * @return 如果是 OFFSET_DATE_TIME，返回 true，否则返回 false。
     */
    inline bool isOffsetDateTime() const noexcept {
        return m_type == TomlDateTimeType::OFFSET_DATE_TIME;
    }

    /**
     * @brief 检查是否为本地日期时间类型。
     * @return 如果是 LOCAL_DATE_TIME，返回 true，否则返回 false。
     */
    inline bool isLocalDateTime() const noexcept {
        return m_type == TomlDateTimeType::LOCAL_DATE_TIME;
    }

    /**
     * @brief 检查是否为本地日期类型。
     * @return 如果是 LOCAL_DATE，返回 true，否则返回 false。
     */
    inline bool isLocalDate() const noexcept {
        return m_type == TomlDateTimeType::LOCAL_DATE;
    }

    /**
     * @brief 检查是否为本地时间类型。
     * @return 如果是 LOCAL_TIME，返回 true，否则返回 false。
     */
    inline bool isLocalTime() const noexcept {
        return m_type == TomlDateTimeType::LOCAL_TIME;
    }

    /**
     * @brief 获取年份（仅对包含日期的类型有效）。
     * @return 年份值（如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getYear() const noexcept {
        // 只有包含日期的类型（LOCAL_DATE/LOCAL_DATE_TIME/OFFSET_DATE_TIME）有年
        if (m_type == TomlDateTimeType::LOCAL_DATE || m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
            m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getSignedBits(m_core, 48, 16));
        }
        return std::nullopt;
    }

    /**
     * @brief 获取月份（仅对包含日期的类型有效）。
     * @return 月份值（1-12，如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getMonth() const noexcept {
        if (m_type == TomlDateTimeType::LOCAL_DATE || m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
            m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getBits(m_core, 44, 4));
        }
        return std::nullopt;
    }

    /**
     * @brief 获取日期（仅对包含日期的类型有效）。
     * @return 日期值（1-31，如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getDay() const noexcept {
        if (m_type == TomlDateTimeType::LOCAL_DATE || m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
            m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getBits(m_core, 39, 5));
        }
        return std::nullopt;
    }

    /**
     * @brief 获取小时（仅对包含时间的类型有效）。
     * @return 小时值（0-23，如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getHour() const noexcept {
        if (m_type == TomlDateTimeType::LOCAL_TIME || m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
            m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getBits(m_core, 34, 5));
        }
        return std::nullopt;
    }

    /**
     * @brief 获取分钟（仅对包含时间的类型有效）。
     * @return 分钟值（0-59，如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getMinute() const noexcept {
        if (m_type == TomlDateTimeType::LOCAL_TIME || m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
            m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getBits(m_core, 28, 6));
        }
        return std::nullopt;
    }

    /**
     * @brief 获取秒数（仅对包含时间的类型有效）。
     * @return 秒数值（0-59，如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getSecond() const noexcept {
        if (m_type == TomlDateTimeType::LOCAL_TIME || m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
            m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getBits(m_core, 22, 6));
        }
        return std::nullopt;
    }

    /**
     * @brief 获取亚秒值（纳秒，仅对包含时间的类型有效）。
     * @return 亚秒值（如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getSubSecond() const noexcept {
        if (m_subSecond != -1 &&  // 有亚秒
            (m_type == TomlDateTimeType::LOCAL_TIME ||
             m_type == TomlDateTimeType::LOCAL_DATE_TIME ||
             m_type == TomlDateTimeType::OFFSET_DATE_TIME)) {
            return static_cast<int>(m_subSecond & 0x3FFFFFFF);  // 取低30位
        }
        return std::nullopt;
    }

    /**
     * @brief 获取时区偏移（仅对 OFFSET_DATE_TIME 有效）。
     * @return 时区偏移值（分钟，如果适用），否则返回 std::nullopt。
     */
    inline std::optional<int> getTzOffset() const noexcept {
        if (m_type == TomlDateTimeType::OFFSET_DATE_TIME) {
            return static_cast<int>(getSignedBits(m_core, 11, 11));
        }
        return std::nullopt;
    }

    /**
     * @brief 重置对象到初始状态。
     * @note 将日期类型设为 INVALID，清空所有存储值。
     */
    void reset() noexcept;

    /**
     * @brief 将对象转换为字符串。
     * @return 日期/时间字符串。
     */
    std::string toString() const noexcept;

    /**
     * @brief 将 OFFSET_DATE_TIME 转换为 std::chrono::system_clock::time_point。
     * @return 系统时间点。
     * @throws TomlException 如果不是 OFFSET_DATE_TIME 类型，抛出异常。
     */
    std::chrono::system_clock::time_point toSystemTimePoint() const;

    /**
     * @brief 比较两个 TomlDate 对象是否相等。
     * @param other 要比较的 TomlDate 对象。
     * @return 如果相等，返回 true，否则返回 false。
     */
    inline bool operator==(const TomlDate& other) const noexcept {
        return m_type == other.m_type && m_core == other.m_core && m_subSecond == other.m_subSecond;
    }

    /**
     * @brief 比较两个 TomlDate 对象是否不相等。
     * @param other 要比较的 TomlDate 对象。
     * @return 如果不相等，返回 true，否则返回 false。
     */
    inline bool operator!=(const TomlDate& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief 输出 TomlDate 对象到流。
     * @param os 输出流。
     * @param tomlDate 要输出的 TomlDate 对象。
     * @return 输出流引用。
     */
    inline friend std::ostream& operator<<(std::ostream& os, const TomlDate& tomlDate) noexcept {
        os << tomlDate.toString();
        return os;
    }

  private:
    /**
     * @brief 解析指定长度的数字。
     * @param sv 输入字符串视图。
     * @param position 当前解析位置（会被更新）。
     * @param count 要解析的数字位数。
     * @return 解析出的整数值（如果成功），否则返回 std::nullopt。
     */
    static std::optional<int>
    ParseDigits(const std::string_view& sv, size_t& position, int count) noexcept;

    /**
     * @brief 在指定位范围存储值。
     * @param target 目标存储变量。
     * @param value 要存储的值。
     * @param startBit 起始位。
     * @param bitCount 位数。
     */
    static void setBits(int64_t& target, int64_t value, int startBit, int bitCount) noexcept {
        int64_t mask = (1LL << bitCount) - 1;  // 生成bitCount位的掩码
        value &= mask;                         // 截断值到bitCount位
        target &= ~(mask << startBit);         // 清除目标位置的旧值
        target |= (value << startBit);         // 写入新值
    }

    /**
     * @brief 从指定位范围提取值。
     * @param source 源数据。
     * @param startBit 起始位。
     * @param bitCount 位数。
     * @return 提取的值。
     */
    static int64_t getBits(int64_t source, int startBit, int bitCount) noexcept {
        int64_t mask = (1LL << bitCount) - 1;
        return (source >> startBit) & mask;
    }

    /**
     * @brief 从指定位范围提取有符号值。
     * @param source 源数据。
     * @param startBit 起始位。
     * @param bitCount 位数。
     * @return 提取的有符号值。
     */
    static int64_t getSignedBits(int64_t source, int startBit, int bitCount) noexcept {
        int64_t raw     = getBits(source, startBit, bitCount);
        int64_t signBit = 1LL << (bitCount - 1);  // 符号位位置
        if (raw & signBit) {                      // 负数（补码）
            return raw - (1LL << bitCount);       // 转换为有符号值
        }
        return raw;  // 正数
    }

    /**
     * @brief 设置年份。
     * @param year 年份值。
     */
    inline void setYear(int year) noexcept {
        // 年：16位，startBit=48，bitCount=16
        setBits(m_core, year, 48, 16);
    }

    /**
     * @brief 设置月份。
     * @param month 月份值。
     */
    inline void setMonth(int month) noexcept {
        // 月：4位，startBit=44，bitCount=4
        setBits(m_core, month, 44, 4);
    }

    /**
     * @brief 设置日期。
     * @param day 日期值。
     */
    inline void setDay(int day) noexcept {
        setBits(m_core, day, 39, 5);
    }

    /**
     * @brief 设置小时。
     * @param hour 小时值。
     */
    inline void setHour(int hour) noexcept {
        setBits(m_core, hour, 34, 5);
    }

    /**
     * @brief 设置分钟。
     * @param minute 分钟值。
     */
    inline void setMinute(int minute) noexcept {
        setBits(m_core, minute, 28, 6);
    }

    /**
     * @brief 设置秒数。
     * @param second 秒数值。
     */
    inline void setSecond(int second) noexcept {
        // 使用setBits设置秒（bit22~bit27）
        setBits(m_core, second, 22, 6);
    }

    /**
     * @brief 设置时区偏移。
     * @param tzOffset 时区偏移值（分钟）。
     */
    inline void setTzOffset(int tzOffset) noexcept {
        // 时区偏移：11位，startBit=11，bitCount=11
        setBits(m_core, tzOffset, 11, 11);
    }

    /**
     * @brief 解析时间部分（hh:mm:ss）。
     * @param sv 输入字符串视图。
     * @param position 当前解析位置（会被更新）。
     * @return 如果解析成功，返回 true，否则返回 false。
     */
    bool parseTimePart(const std::string_view& sv, size_t& position) noexcept;

    /**
     * @brief 解析亚秒部分。
     * @param sv 输入字符串视图。
     * @param position 当前解析位置（会被更新）。
     * @throws TomlException 如果解析失败，抛出异常。
     */
    void parseSubSecond(const std::string_view& sv, size_t& position);

    /**
     * @brief 解析时区偏移部分。
     * @param sv 输入字符串视图。
     * @param position 当前解析位置（会被更新）。
     * @throws TomlException 如果解析失败，抛出异常。
     */
    void parseTimezoneOffset(const std::string_view& sv, size_t& position);

    /**
     * @brief 解析本地时间格式。
     * @param sv 输入字符串视图。
     * @return 如果解析成功，返回 true，否则返回 false。
     */
    bool parseLocalTime(const std::string_view& sv) noexcept;

    /**
     * @brief 解析日期时间格式。
     * @param sv 输入字符串视图。
     * @return 如果解析成功，返回 true，否则返回 false。
     * @throws TomlException 如果解析失败，抛出异常。
     */
    bool parseDateTime(const std::string_view& sv);

    /**
     * @brief 解析 TOML 格式的日期/时间字符串。
     * @param s 输入字符串视图。
     * @throws TomlException 如果解析失败，抛出异常。
     */
    void parse(const std::string_view& s);

  private:
    TomlDateTimeType m_type;          ///< 日期/时间类型。
    int64_t          m_core{0};       ///< 存储年月日时分秒和时区偏移。
    int64_t          m_subSecond{0};  ///< 存储亚秒值（纳秒），0 表示无亚秒。
};

/**
 * @struct HasToToml
 * @brief 模板元编程工具，用于检查类型是否支持 toToml 序列化函数。
 *
 * 默认情况下，HasToToml 继承自 std::false_type，表示类型 T 不支持 toToml 函数。
 * @tparam T 要检查的类型。
 * @tparam void 辅助模板参数，用于 SFINAE。
 */
template <typename T, typename = void>
struct HasToToml : std::false_type {};

/**
 * @struct HasToToml&lt;T, std::void_t&lt;decltype(toToml(std::declval&lt;const T&&gt;()))&gt;&gt;
 * @brief HasToToml 的特化版本，检查类型是否具有有效的 toToml 函数。
 *
 * 如果类型 T 具有 toToml 函数且返回值类型为 TomlValue，则继承自 std::true_type。
 * @tparam T 要检查的类型。
 */
template <typename T>
struct HasToToml<T, std::void_t<decltype(toToml(std::declval<const T&>()))>>
    : std::is_same<decltype(toToml(std::declval<const T&>())), TomlValue> {};

/**
 * @struct HasFromToml
 * @brief 模板元编程工具，用于检查类型是否支持 fromToml 反序列化函数。
 *
 * 默认情况下，HasFromToml 继承自 std::false_type，表示类型 T 不支持 fromToml 函数。
 * @tparam T 要检查的类型。
 * @tparam void 辅助模板参数，用于 SFINAE。
 */
template <typename T, typename = void>
struct HasFromToml : std::false_type {};

/**
 * @struct HasFromToml&lt;T, std::void_t&lt;decltype(fromToml(std::declval&lt;const
 * TomlValue&&gt;(), std::declval&lt;T&>()))&gt;&gt;
 * @brief HasFromToml 的特化版本，检查类型是否具有有效的 fromToml 函数。
 *
 * 如果类型 T 具有 fromToml 函数且返回值类型为 void，则继承自 std::true_type。
 * @tparam T 要检查的类型。
 */
template <typename T>
struct HasFromToml<
    T,
    std::void_t<decltype(fromToml(std::declval<const TomlValue&>(), std::declval<T&>()))>>
    : std::is_same<decltype(fromToml(std::declval<const TomlValue&>(), std::declval<T&>())), void> {
};

// 容器序列化支持
/**
 * @brief Toml 对象反序列化为 std::vector
 * @tparam T 向量元素的类型
 * @param root 要反序列化的 Toml 对象
 * @param vec 反序列化后的 std::vector 对象
 * @note 如果 T 支持 fromToml函数，则使用 fromToml 进行反序列化；否则直接构造 TomlValue
 */
template <typename T>
void fromToml(const TomlValue& root, std::vector<T>& vec);

/**
 * @brief 将 Toml 对象反序列化为键为字符串的映射
 * @tparam Map 映射类型（std::map 或 std::unordered_map）
 * @tparam T 映射值的类型
 * @tparam S SFINAE 约束，确保键类型为 std::string 且 Map 为 std::map 或 std::unordered_map
 * @param root 要反序列化的 Toml 对象
 * @param map 反序列化后的映射对象
 * @note 如果值类型支持 fromToml 函数，则使用 fromToml 反序列化；否则直接构造 TomlValue
 */
template <typename Map,
          typename T = typename Map::mapped_type,
          typename S = std::enable_if_t<std::is_same_v<typename Map::key_type, std::string> &&
                                        (std::is_same_v<Map, std::map<std::string, T>> ||
                                         std::is_same_v<Map, std::unordered_map<std::string, T>>)>>
void fromToml(const TomlValue& root, Map& map);

/**
 * @brief 将 std::vector 序列化为 Toml 数组。
 * @tparam T 向量元素的类型。
 * @param vec 要序列化的向量。
 * @return 表示 Toml 数组的 TomlValue 对象。
 * @note 如果元素类型支持 toToml 函数，则使用 toToml 序列化；否则直接构造 TomlValue。
 */
template <typename T>
TomlValue toToml(const std::vector<T>& vec);

/**
 * @brief 将键为字符串的映射序列化为 Toml 对象。
 * @tparam Map 映射类型（std::map 或 std::unordered_map）
 * @tparam T 映射值的类型。
 * @tparam S SFINAE 约束，确保键类型为 std::string 且 Map 为 std::map 或 std::unordered_map。
 * @param map 要序列化的映射。
 * @return 表示 Toml 对象的 TomlValue 对象。
 * @note 如果值类型支持 toToml 函数，则使用 toToml 序列化；否则直接构造 TomlValue。
 */
template <typename Map,
          typename T = typename Map::mapped_type,
          typename S = std::enable_if_t<std::is_same_v<typename Map::key_type, std::string> &&
                                        (std::is_same_v<Map, std::map<std::string, T>> ||
                                         std::is_same_v<Map, std::unordered_map<std::string, T>>)>>
TomlValue toToml(const Map& map);

/**
 * @class TomlValue
 * @brief 表示 TOML 格式的值。
 *
 * 支持 TOML
 * 规范中的所有数据类型（布尔值、整数、浮点数、字符串、日期、数组、对象），提供构造、访问和序列化功能。
 */
class TomlValue {
  public:
    /**
     * @brief 默认构造函数，初始化为空对象。
     */
    TomlValue() : m_type(TomlType::Object) {
        m_value.object = new TomlObject();
    }

    /**
     * @brief 构造布尔值类型的 TomlValue。
     * @param value 布尔值。
     */
    TomlValue(bool value) : m_type(TomlType::Boolean) {
        m_value.boolean = value;
    }

    /**
     * @brief 构造整数类型的 TomlValue。
     * @tparam T 整数类型。
     * @param value 整数值。
     */
    template <typename T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    TomlValue(T value) : m_type(TomlType::Integer) {
        m_value.iNumber = static_cast<int64_t>(value);
    }

    /**
     * @brief 构造浮点数类型的 TomlValue。
     * @tparam T 浮点数类型。
     * @param value 浮点数值。
     */
    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    TomlValue(T value) : m_type(TomlType::Double) {
        m_value.dNumber = static_cast<double>(value);
    }

    /**
     * @brief 构造字符串类型的 TomlValue。
     * @tparam T 可转换为 std::string_view 的类型。
     * @param value 字符串值。
     */
    template <typename T,
              std::enable_if_t<std::is_convertible_v<std::decay_t<T>, std::string_view> &&
                                   !std::is_same_v<std::decay_t<T>, TomlValue>,
                               int> = 0>
    TomlValue(T value) : m_type(TomlType::String) {
        m_value.string = new TomlString(value);
    }

    /**
     * @brief 构造字符串类型的 TomlValue（移动构造）
     * @param value 字符串值（右值）。
     */
    TomlValue(TomlString&& value) : m_type(TomlType::String) {
        m_value.string = new TomlString(std::move(value));
    }

    /**
     * @brief 构造日期类型的 TomlValue。
     * @param date TomlDate 对象。
     */
    TomlValue(const TomlDate& date) : m_type(TomlType::Date) {
        m_value.date = new TomlDate(date);
    }

    /**
     * @brief 构造数组类型的 TomlValue。
     * @param value TomlArray 对象。
     */
    TomlValue(const TomlArray& value) noexcept : m_type(TomlType::Array) {
        m_value.array = new TomlArray(value);
    }

    /**
     * @brief 构造对象类型的 TomlValue。
     * @param value TomlObject 对象。
     */
    TomlValue(const TomlObject& value) noexcept : m_type(TomlType::Object) {
        m_value.object = new TomlObject(value);
    }

    /**
     * @brief 构造数组类型的 TomlValue（从 std::vector）
     * @tparam T 向量元素的类型。
     * @param vec 向量对象。
     */
    template <typename T>
    TomlValue(const std::vector<T>& vec) : m_type(TomlType::Array) {
        *this = toToml(vec);
    }

    /**
     * @brief 构造对象类型的 TomlValue（从 std::map）
     * @tparam T 映射值的类型。
     * @param map 映射对象。
     */
    template <typename T>
    TomlValue(const std::map<std::string, T>& map) : m_type(TomlType::Object) {
        *this = toToml(map);
    }

    /**
     * @brief 构造对象类型的 TomlValue（从 std::unordered_map）
     * @tparam T 映射值的类型。
     * @param map 无序映射对象。
     */
    template <typename T>
    TomlValue(const std::unordered_map<std::string, T>& map) : m_type(TomlType::Object) {
        *this = toToml(map);
    }

    /**
     * @brief 拷贝构造函数。
     * @param other 要拷贝的 TomlValue 对象。
     */
    TomlValue(const TomlValue& other);

    /**
     * @brief 移动构造函数。
     * @param other 要移动的 TomlValue 对象。
     */
    TomlValue(TomlValue&& other) noexcept;

    /**
     * @brief 使用初始化列表构造对象类型的 Toml 数据。
     * @tparam T 值类型。
     * @param init 初始化列表，包含键值对（const char*, T）
     */
    template <typename T>
    TomlValue(std::initializer_list<std::pair<const char*, T>> init) : m_type(TomlType::Object) {
        m_value.object = new TomlObject();
        for (const auto& [k, v] : init) {
            (*m_value.object)[k] = TomlValue(v);
        }
    }

    /**
     * @brief 使用初始化列表构造对象类型的 Toml 数据（TomlValue 值）
     * @param init 初始化列表，包含键值对（const char*, TomlValue）
     */
    TomlValue(std::initializer_list<std::pair<const char*, TomlValue>> init)
        : m_type(TomlType::Object) {
        m_value.object = new TomlObject();
        for (const auto& [k, v] : init) {
            (*m_value.object)[k] = v;
        }
    }

    /**
     * @brief 使用初始化列表构造对象类型的 Toml 数据（通用类型，转换为键值对）
     * @tparam T 初始化列表元素类型（需可转换为 std::pair&lt;const char*, TomlValue&gt;）
     * @param init 初始化列表。
     */
    template <
        typename T,
        std::enable_if_t<std::is_convertible_v<T, std::pair<const char*, TomlValue>>, int> = 0>
    TomlValue(std::initializer_list<T> init) {
        // 对象初始化
        m_type         = TomlType::Object;
        m_value.object = new TomlObject();
        for (const auto& item : init) {
            auto pair                     = static_cast<std::pair<const char*, TomlValue>>(item);
            (*m_value.object)[pair.first] = pair.second;
        }
    }

    /**
     * @brief 拷贝赋值运算符。
     * @param other 要拷贝的 TomlValue 对象。
     * @return 自身引用。
     */
    TomlValue& operator=(const TomlValue& other);

    /**
     * @brief 移动赋值运算符。
     * @param other 要移动的 TomlValue 对象。
     * @return 自身引用。
     */
    TomlValue& operator=(TomlValue&& other) noexcept;

    /**
     * @brief 使用初始化列表为数组赋值
     * @param init 初始化列表，包含 TomlValue 元素
     * @return 自身引用
     * @note 释放现有数据并构造新的 Toml 数组
     */
    TomlValue& operator=(std::initializer_list<TomlValue> init) {
        destroyValue();
        m_type        = TomlType::Array;
        m_value.array = new TomlArray(init);
        return *this;
    }

    /**
     * @brief 使用初始化列表为对象赋值（键值对）
     * @tparam T 初始化列表元素类型（需可转换为 std::pair&lt;const char*, TomlValue&gt;）
     * @param init 初始化列表，包含键值对。
     * @return 自身引用。
     */
    template <
        typename T,
        std::enable_if_t<std::is_convertible_v<T, std::pair<const char*, TomlValue>>, int> = 0>
    TomlValue& operator=(std::initializer_list<T> init) {
        // 释放之前的数据
        destroyValue();
        // 对象赋值
        m_type         = TomlType::Object;
        m_value.object = new TomlObject();
        for (const auto& [k, v] : init) {
            (*m_value.object)[k] = v;
        }
        return *this;
    }

    /**
     * @brief 使用初始化列表为数组赋值（非键值对）
     * @tparam T 初始化列表元素类型（非需不可转换为 std::pair&lt;const char*, TomlValue&gt;）
     * @param init 初始化列表，包含数组元素。
     * @return 自身引用。
     */
    template <
        typename T,
        std::enable_if_t<!std::is_convertible_v<T, std::pair<const char*, TomlValue>>, int> = 0>
    TomlValue& operator=(std::initializer_list<T> init) {
        // 释放之前的数据
        destroyValue();
        // 数组赋值
        m_type        = TomlType::Array;
        m_value.array = new TomlArray();
        m_value.array->reserve(init.size());
        for (const auto& item : init) {
            m_value.array->emplace_back(TomlValue(item));
        }
        return *this;
    }

    /**
     * @brief 析构函数，释放内部资源。
     */
    ~TomlValue();

    /**
     * @brief 获取当前值的数据类型。
     * @return TomlType 枚举值，表示当前值的类型。
     */
    inline TomlType type() const noexcept {
        return m_type;
    }

    /**
     * @brief 检查是否为布尔类型。
     * @return 如果是布尔值，返回 true，否则返回 false。
     */
    inline bool isBoolean() const noexcept {
        return m_type == TomlType::Boolean;
    }

    /**
     * @brief 检查是否为数值类型（整数或浮点数）
     * @return 如果是数值类型，返回 true，否则返回 false。
     */
    inline bool isNumber() const noexcept {
        return m_type == TomlType::Integer || m_type == TomlType::Double;
    }

    /**
     * @brief 检查是否为字符串类型。
     * @return 如果是字符串，返回 true，否则返回 false。
     */
    inline bool isString() const noexcept {
        return m_type == TomlType::String;
    }

    /**
     * @brief 检查是否为日期类型。
     * @return 如果是日期类型，返回 true，否则返回 false。
     */
    inline bool isDate() const noexcept {
        return m_type == TomlType::Date;
    }

    /**
     * @brief 检查是否为数组类型。
     * @return 如果是数组，返回 true，否则返回 false。
     */
    inline bool isArray() const noexcept {
        return m_type == TomlType::Array;
    }

    /**
     * @brief 检查是否为对象类型。
     * @return 如果是对象，返回 true，否则，返回 false。
     */
    inline bool isObject() const noexcept {
        return m_type == TomlType::Object;
    }

    /**
     * @brief 转换为布尔值。
     * @exception TomlException 如果不是布尔类型，则抛出异常。
     * @return 布尔值。
     */
    operator bool() const;

    /**
     * @brief 转换为 16 位整数。
     * @exception TomlException 如果不是数值类型，则抛出异常。
     * @return 16 位整数值。
     */
    operator int16_t() const;

    /**
     * @brief 转换为 32 位整数。
     * @exception TomlException 如果不是数值类型，则抛出异常。
     * @return 32 位整数值。
     */
    operator int32_t() const;

    /**
     * @brief 转换为 64 位整数。
     * @exception TomlException 如果不是数值类型，则抛出异常。
     * @return 64 位整数值。
     */
    operator int64_t() const;

    /**
     * @brief 转换为浮点数（float）
     * @exception TomlException 如果不是数值类型，则抛出异常。
     * @return 浮点数值（float）
     */
    operator float() const;

    /**
     * @brief 转换为浮点数（double）
     * @exception TomlException 如果不是数值类型，则抛出异常。
     * @return 浮点数值（double）
     */
    operator double() const;

    /**
     * @brief 转换为字符串。
     * @exception TomlException 如果不是字符串类型，则抛出异常。
     * @return 字符串值。
     */
    operator std::string() const;

    /**
     * @brief 获取指定类型的值。
     * @tparam T 目标类型。
     * @return 转换后的值。
     * @note 如果类型不匹配或无法转换，无法编译通过。
     */
    template <typename T>
    T get() const {
        using rawT = T;
        if constexpr (std::is_same_v<rawT, TomlValue>) {
            // 本身
            return *this;
        } else if constexpr (std::is_arithmetic_v<rawT>) {
            // 支持所有数值类型之间的转换
            if (!(isNumber() || isBoolean())) {
                throw TomlException("Cannot convert to numeric type");
            }
            if (m_type == TomlType::Integer) {
                return static_cast<rawT>(m_value.iNumber);
            } else if (m_type == TomlType::Boolean) {
                return m_value.boolean;
            } else {
                return static_cast<rawT>(m_value.dNumber);
            }
        } else if constexpr (std::is_same_v<rawT, std::string> ||
                             std::is_same_v<rawT, std::string_view>) {
            // 字符串
            return operator std::string();
        } else if constexpr (std::is_same_v<rawT, TomlDate>) {
            // 日期
            return asDate();
        } else if constexpr (HasFromToml<rawT>::value) {
            // 处理自定义类型
            rawT result;
            fromToml(*this, result);
            return result;
        } else {
            // 这行永远不会执行，只是为了避免编译器警告
            static_assert(std::is_void_v<rawT>, "Unsupported type for get<T>()");
            return rawT{};
        }
    }

    /**
     * @brief 访问数组元素（读写）
     * @tparam T 索引类型（必须为整数）
     * @param key 数组索引。
     * @return 数组中对应索引的 TomlValue 引用。
     * @throws TomlException 如果索引为负数或不是数组类型，抛出异常。
     */
    template <typename T, std::enable_if_t<std::is_integral_v<std::remove_reference_t<T>>, int> = 0>
    TomlValue& operator[](T&& key) {
        // key为数字
        auto& array = asArray();
        if (key >= 0) {
            if (static_cast<size_t>(key) >= array.size()) {
                array.resize(key + 1);
            }
            return array[key];
        }
        throw TomlException("Negative Array index");
    }

    /**
     * @brief 访问对象属性（读写）
     * @tparam T 键类型（字符串或可转换为字符串）
     * @param key 对象键。
     * @return 对象中对应键的 TomlValue 引用。
     * @throws TomlException 如果不是对象类型，抛出异常。
     */
    template <typename T,
              std::enable_if_t<
                  !std::is_integral_v<std::remove_reference_t<T>> &&
                      (std::is_convertible_v<T, std::string> ||
                       std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, const char*>),
                  int> = 0>
    TomlValue& operator[](T&& key) {
        auto& object = asObject();
        return object[std::forward<T>(key)];
    }

    /**
     * @brief 访问数组元素（只读）
     * @tparam T 索引类型（必须为整数）
     * @param key 数组索引。
     * @return 数组中对应索引的 TomlValue 常量引用。
     * @throws TomlException 如果索引超出范围或不是数组类型，抛出异常。
     */
    template <typename T, std::enable_if_t<std::is_integral_v<std::remove_reference_t<T>>, int> = 0>
    const TomlValue& operator[](T&& key) const {
        auto& array = asArray();
        if (key >= 0 && static_cast<size_t>(key) < array.size()) {
            return array[key];
        }
        throw TomlException("Array index out of range");
    }

    /**
     * @brief 访问对象属性（只读）
     * @tparam T 键类型（字符串或可转换为字符串）
     * @param key 对象键。
     * @return 对象中对应键的 TomlValue 常量引用。
     * @throws TomlException 如果键不存在或不是对象类型，抛出异常。
     */
    template <typename T,
              std::enable_if_t<
                  !std::is_integral_v<std::remove_reference_t<T>> &&
                      (std::is_convertible_v<T, std::string> ||
                       std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, const char*>),
                  int> = 0>
    const TomlValue& operator[](T&& key) const {
        const auto& object = asObject();
        auto        it     = object.find(std::forward<T>(key));
        if (it != object.end()) {
            return it->second;
        }
        throw TomlException("Key not found");
    }

    /**
     * @brief 设置对象键值对。
     * @param key 键名。
     * @param value 要设置的 TomlValue 值。
     * @return 自身引用。
     * @note 如果当前不是对象类型，会转换为对象类型。
     */
    TomlValue& insert(const std::string& key, const TomlValue& value);

    /**
     * @brief 向数组添加元素。
     * @param value 要添加的 TomlValue 值。
     * @return 自身引用。
     * @note 如果当前不是数组类型，会转换为数组类型。
     */
    TomlValue& push_back(const TomlValue& value);

    /**
     * @brief 获取字符串值（读写）。
     * @return 字符串引用。
     * @throws TomlException 如果不是字符串类型，抛出异常。
     */
    TomlString& asString() {
        if (m_type != TomlType::String) {
            throw TomlException("not a string");
        }
        return *m_value.string;
    }

    /**
     * @brief 获取字符串值（只读）。
     * @return 字符串常量引用。
     * @throws TomlException 如果不是字符串类型，抛出异常。
     */
    const TomlString& asString() const {
        if (m_type != TomlType::String) {
            throw TomlException("not a string");
        }
        return *m_value.string;
    }

    /**
     * @brief 获取日期值（读写）。
     * @return 日期引用。
     * @throws TomlException 如果不是日期类型，抛出异常。
     */
    TomlDate& asDate() {
        if (m_type != TomlType::Date) {
            throw TomlException("not a date");
        }
        return *m_value.date;
    }

    /**
     * @brief 获取日期值（只读）。
     * @return 日期常量引用。
     * @throws TomlException 如果不是日期类型，抛出异常。
     */
    const TomlDate& asDate() const {
        if (m_type != TomlType::Date) {
            throw TomlException("not a date");
        }
        return *m_value.date;
    }

    /**
     * @brief 获取数组值（读写）。
     * @return 数组引用。
     * @throws TomlException 如果不是数组类型，抛出异常。
     */
    TomlArray& asArray() {
        if (m_type != TomlType::Array) {
            throw TomlException("not a array");
        }
        return *m_value.array;
    }

    /**
     * @brief 获取数组值（只读）。
     * @return 数组常量引用。
     * @throws TomlException 如果不是数组类型，抛出异常。
     */
    const TomlArray& asArray() const {
        if (m_type != TomlType::Array) {
            throw TomlException("not a array");
        }
        return *m_value.array;
    }

    /**
     * @brief 获取对象值（读写）。
     * @return 对象引用。
     * @throws TomlException 如果不是对象类型，抛出异常。
     */
    TomlObject& asObject() {
        if (m_type != TomlType::Object) {
            throw TomlException("not a object");
        }
        return *m_value.object;
    }

    /**
     * @brief 获取对象值（只读）。
     * @return 对象常量引用。
     * @throws TomlException 如果不是对象类型，抛出异常。
     */
    const TomlObject& asObject() const {
        if (m_type != TomlType::Object) {
            throw TomlException("not a object");
        }
        return *m_value.object;
    }

    /**
     * @brief 将 TomlValue 转换为字符串表示。
     * @param type 序列化格式（TO_TOML、TO_JSON 或 TO_YAML）。
     * @param indent 缩进空格数（仅对 JSON 和 YAML 有效）。
     * @return 序列化后的字符串。
     */
    inline std::string toString(parser::StringifyType type   = parser::StringifyType::TO_TOML,
                                int                   indent = 0) const {
        return parser::stringify(*this, type, indent);
    }

    /**
     * @brief 输出 TomlValue 到流。
     * @param os 输出流。
     * @param value 要输出的 TomlValue 对象。
     * @return 输出流引用。
     * @note 只会输出toml字符串到输出流。
     */
    friend std::ostream& operator<<(std::ostream& os, const TomlValue& value) {
        os << value.toString();
        return os;
    }

  private:
    /**
     * @brief 释放内部资源。
     */
    void destroyValue() noexcept;

  private:
    TomlType m_type{};  ///< 当前值的数据类型。
    union
    {
        bool        boolean;  ///< 布尔值。
        int64_t     iNumber;  ///< 整数值。
        double      dNumber;  ///< 浮点值。
        TomlString* string;   ///< 字符串指针。
        TomlDate*   date;     ///< 日期指针。
        TomlArray*  array;    ///< 数组指针。
        TomlObject* object;   ///< 对象指针。
    } m_value{};              ///< 存储值的联合体。
};

// 容器序列化支持

template <typename T>
void fromToml(const TomlValue& root, std::vector<T>& vec) {
    if (!root.isArray()) {
        throw TomlException("Not an Array");
    }
    using ValueType = typename std::vector<T>::value_type;
    vec.clear();
    vec.reserve(root.asArray().size());
    for (const auto& item : root.asArray()) {
        if constexpr (HasFromToml<ValueType>::value) {
            ValueType value;
            fromToml(item, value);
            vec.emplace_back(std::move(value));
        } else {
            vec.emplace_back(item.template get<ValueType>());
        }
    }
}

template <typename Map, typename T, typename>
void fromToml(const TomlValue& root, Map& map) {
    if (!root.isObject()) {
        throw TomlException("Not a Object");
    }
    using ValueType = typename Map::mapped_type;
    map.clear();
    for (const auto& [key, item] : root.asObject()) {
        if constexpr (HasToToml<ValueType>::value) {
            ValueType value;
            fromToml(item, value);
            map[key] = value;
        } else {
            // 没有定义fromToml函数
            map[key] = item.template get<ValueType>();
        }
    }
}

template <typename T>
TomlValue toToml(const std::vector<T>& vec) {
    std::vector<TomlValue> result;
    result.reserve(vec.size());
    for (const auto& item : vec) {
        if constexpr (HasToToml<T>::value) {
            result.emplace_back(toToml(item));
        } else {
            result.emplace_back(item);
        }
    }
    return result;
}

template <typename Map, typename T, typename>
TomlValue toToml(const Map& map) {
    TomlObject result;
    for (const auto& [key, value] : map) {
        if constexpr (HasToToml<T>::value) {
            result[key] = toToml(value);
        } else {
            result[key] = TomlValue(value);
        }
    }
    return result;
}

/**
 * @brief 将字符串字面量转为TomlValue
 * @param data 字符串指针
 * @param length 字符串长度
 * @return 解析后的TomlValue
 * @note 解析调用TomlParser::parse且不支持任何扩展
 */
inline TomlValue operator""_Toml(const char* data, size_t length) {
    return parser::parse({data, length});
}

}  // namespace cctoml

#endif
#pragma clang diagnostic pop
