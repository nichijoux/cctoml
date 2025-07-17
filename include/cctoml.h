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
class TomlException : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class TomlParseException : public std::runtime_error {
  public:
    TomlParseException(const std::string& data, size_t position)
        : std::runtime_error(data + ", position: " + std::to_string(position)) {}
};

enum class TomlType { Boolean, Integer, Double, String, Date, Array, Object };

class TomlValue;

namespace parser {
    TomlValue Parse(std::string_view data);

    std::string Stringify(const TomlValue& value);
}  // namespace parser

using TomlString = std::string;
using TomlArray  = std::vector<TomlValue>;
using TomlObject = std::map<TomlString, TomlValue>;

class TomlDate {
  public:
    enum class TomlDateTimeType {
        INVALID,
        OFFSET_DATE_TIME,
        LOCAL_DATE_TIME,
        LOCAL_DATE,
        LOCAL_TIME
    };

  public:
    // 从符合 TOML 规范的字符串构造对象
    TomlDate(std::string_view s) : m_type(TomlDateTimeType::INVALID) {
        parse(std::string(s));
    }

    inline TomlDateTimeType type() const {
        return m_type;
    }

    inline bool isOffsetDateTime() const {
        return m_type == TomlDateTimeType::OFFSET_DATE_TIME;
    }

    inline bool isLocalDateTime() const {
        return m_type == TomlDateTimeType::LOCAL_DATE_TIME;
    }

    inline bool isLocalDate() const {
        return m_type == TomlDateTimeType::LOCAL_DATE;
    }

    inline bool isLocalTime() const {
        return m_type == TomlDateTimeType::LOCAL_TIME;
    }

    // 获取日期时间的各个部分
    inline std::optional<int> getYear() const {
        return m_year;
    }

    inline std::optional<int> getMonth() const {
        return m_month;
    }

    inline std::optional<int> getDay() const {
        return m_day;
    }

    inline std::optional<int> getHour() const {
        return m_hour;
    }

    inline std::optional<int> getMinute() const {
        return m_minute;
    }

    inline std::optional<int> getSecond() const {
        return m_second;
    }

    inline std::optional<std::chrono::nanoseconds> getSubSecond() const {
        return m_subSecond;
    }

    inline std::optional<std::chrono::minutes> getOffset() const {
        return m_tzOffset;
    }

    void reset();

    // 将对象转换回 TOML 格式的字符串
    std::string toString() const;

    // 将 OffsetDateTime 转换为 std::chrono::system_clock::time_point
    // 注意: 只有 OffsetDateTime 类型可以被无歧义地转换
    // 对其他类型调用此函数会抛出 std::logic_error
    std::chrono::system_clock::time_point toSystemTimePoint() const;

    // 比较操作符
    inline bool operator==(const TomlDate& other) const {
        return m_type == other.m_type && m_year == other.m_year && m_month == other.m_month &&
               m_day == other.m_day && m_hour == other.m_hour && m_minute == other.m_minute &&
               m_second == other.m_second && m_subSecond == other.m_subSecond &&
               m_tzOffset == other.m_tzOffset;
    }

    inline bool operator!=(const TomlDate& other) const {
        return !(*this == other);
    }

    inline friend std::ostream& operator<<(std::ostream& os, const TomlDate& tomlDate) {
        os << tomlDate.toString();
        return os;
    }

  private:
    static std::optional<int> parseDigits(const std::string_view& sv, size_t& position, int count);

  private:
    bool parseTimePart(const std::string_view& sv, size_t& position);

    void parseSubSecond(const std::string_view& sv, size_t& position);

    void parseTimezoneOffset(const std::string_view& sv, size_t& position);

    bool parseLocalTime(const std::string_view& sv);

    bool parseDateTimeWithDate(const std::string_view& sv);

    void parse(const std::string& s);

  private:
    // 详细的日期类型
    TomlDateTimeType m_type;
    // 年、月、日
    std::optional<int> m_year, m_month, m_day;
    // 时、分、秒
    std::optional<int> m_hour, m_minute, m_second;
    // subsecond
    std::optional<std::chrono::nanoseconds> m_subSecond;
    // 时区偏移
    std::optional<std::chrono::minutes> m_tzOffset;
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

class TomlValue {
  public:
    TomlValue() : m_type(TomlType::Object) {
        m_value.object = new TomlObject();
    }

    TomlValue(bool value) : m_type(TomlType::Boolean) {
        m_value.boolean = value;
    }

    template <typename T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    TomlValue(T value) : m_type(TomlType::Integer) {
        m_value.iNumber = static_cast<int64_t>(value);
    }

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    TomlValue(T value) : m_type(TomlType::Double) {
        m_value.dNumber = static_cast<double>(value);
    }

    template <typename T,
              std::enable_if_t<std::is_convertible_v<std::decay_t<T>, std::string_view> &&
                                   !std::is_same_v<std::decay_t<T>, TomlValue>,
                               int> = 0>
    TomlValue(T value) : m_type(TomlType::String) {
        m_value.string = new TomlString(value);
    }

    TomlValue(TomlString&& value) : m_type(TomlType::String) {
        m_value.string = new TomlString(std::move(value));
    }

    TomlValue(const TomlDate& date) : m_type(TomlType::Date) {
        m_value.date = new TomlDate(date);
    }

    /**
     * @brief 构造数组类型的 Toml 数据。
     * @param value Toml 数组对象。
     */
    TomlValue(const TomlArray& value) noexcept : m_type(TomlType::Array) {
        m_value.array = new TomlArray(value);
    }

    /**
     * @brief 构造对象类型的 Toml 数据。
     * @param value Toml 对象（键值对映射）
     */
    TomlValue(const TomlObject& value) noexcept : m_type(TomlType::Object) {
        m_value.object = new TomlObject(value);
    }

    /**
     * @brief 构造数组类型的 Toml 数据（std::vector）
     * @tparam T 向量元素的类型。
     * @param vec 向量对象。
     */
    template <typename T>
    TomlValue(const std::vector<T>& vec) : m_type(TomlType::Array) {
        *this = toToml(vec);
    }

    /**
     * @brief 构造对象类型的 Toml 数据（std::map）
     * @tparam T 映射值的类型。
     * @param map 映射对象。
     */
    template <typename T>
    TomlValue(const std::map<std::string, T>& map) : m_type(TomlType::Object) {
        *this = toToml(map);
    }

    /**
     * @brief 构造对象类型的 Toml 数据（std::unordered_map）
     * @tparam T 映射值的类型。
     * @param map 无序映射对象。
     */
    template <typename T>
    TomlValue(const std::unordered_map<std::string, T>& map) : m_type(TomlType::Object) {
        *this = toToml(map);
    }

    /**
     * @brief 拷贝构造函数。
     */
    TomlValue(const TomlValue& other);

    /**
     * @brief 移动构造函数。
     * @param other TomlValue 对象（右值）
     */
    TomlValue(TomlValue&& other) noexcept;

    /**
     * @brief 拷贝赋值运算符。
     * @param other 要赋值的 TomlValue 对象。
     * @return 自身引用。
     */
    TomlValue& operator=(const TomlValue& other);

    /**
     * @brief 移动赋值运算符。
     * @param other TomlValue 对象（右值）
     * @return 自身引用。
     */
    TomlValue& operator=(TomlValue&& other) noexcept;

    /**
     * @brief 析构函数，释放内部资源。
     */
    ~TomlValue();

    /**
     * @brief 获取 Toml 数据类型。
     * @return 当前 Toml 值的类型（TomlType）
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

    template <typename T>
    T get() const {
        using rawT = T;
        if constexpr (std::is_same_v<rawT, TomlValue>) {
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
            return operator std::string();
        } else if constexpr (std::is_same_v<rawT, TomlDate>) {
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
     * @brief 访问数组元素（可读写）
     * @param T 数组索引（整数）
     * @param key 参数类型（整数）
     * @return 数组中对应索引的 TomlValue 引用。
     * @exception std::out_of_range 如果索引为负数。
     */
    template <typename T, std::enable_if_t<std::is_integral_v<std::remove_reference_t<T>>, int> = 0>
    TomlValue& operator[](T&& key) {
        // key为数字
        auto& arr = asArray();
        if (key >= 0) {
            if (static_cast<size_t>(key) >= arr.size()) {
                arr.resize(key + 1);
            }
            return arr[key];
        }
        throw TomlException("Negative Array index");
    }

    /**
     * @brief 访问对象属性（可读写）
     * @param T 键类型（字符串或可转换为字符串）
     * @param key 参数类型 T。
     * @return 对象中对应键的 TomlValue 引用。
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
     * @brief 访问数组元素（只读读）
     * @param T 数组索引（整数类型）
     * @param key 参数类型 T。
     * @return 数组中对应索引的 TomlValue 常量引用值。
     * @exception std::out_of范围 如果索引超出范围。
     * @exception std::runtime_error 如果不是数组。
     */
    template <typename T, std::enable_if_t<std::is_integral_v<std::remove_reference_t<T>>, int> = 0>
    const TomlValue& operator[](T&& key) const {
        auto& arr = asArray();
        if (key >= 0 && static_cast<size_t>(key) < arr.size()) {
            return arr[key];
        }
        throw TomlException("Array index out of range");
    }

    /**
     * @brief 访问对象属性（只读）
     * @param key 参数类型 T 键类型（字符串或 C 风格字符串）
     * @return 对象中对应键的 TomlValue 常量引用值。
     * @exception std::out_of范围 如果键不存在。
     * @exception std::runtime_error 如果不是对象。
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

    TomlValue& set(const std::string& key, const TomlValue& value);

    TomlValue& push_back(const TomlValue& value);

    TomlString& asString() {
        if (m_type != TomlType::String) {
            throw TomlException("not a string");
        }
        return *m_value.string;
    }

    const TomlString& asString() const {
        if (m_type != TomlType::String) {
            throw TomlException("not a string");
        }
        return *m_value.string;
    }

    TomlDate& asDate() {
        if (m_type != TomlType::Date) {
            throw TomlException("not a date");
        }
        return *m_value.date;
    }

    const TomlDate& asDate() const {
        if (m_type != TomlType::Date) {
            throw TomlException("not a date");
        }
        return *m_value.date;
    }

    TomlArray& asArray() {
        if (m_type != TomlType::Array) {
            throw TomlException("not a array");
        }
        return *m_value.array;
    }

    const TomlArray& asArray() const {
        if (m_type != TomlType::Array) {
            throw TomlException("not a array");
        }
        return *m_value.array;
    }

    TomlObject& asObject() {
        if (m_type != TomlType::Object) {
            throw TomlException("not a object");
        }
        return *m_value.object;
    }

    const TomlObject& asObject() const {
        if (m_type != TomlType::Object) {
            throw TomlException("not a object");
        }
        return *m_value.object;
    }

    /**
     * @brief 将 Toml 值转换为字符串表示。
     * @param indent 缩进空格数（默认 0，表示无缩进）
     * @return Toml 值的字符串表示。
     */
    inline std::string toString() const {
        return parser::Stringify(*this);
    }

    /**
     * @brief 输出 Toml 值到流。
     * @param os 输出流。
     * @param value Toml 值。
     * @return 输出流引用。
     */
    friend std::ostream& operator<<(std::ostream& os, const TomlValue& value) {
        os << value.toString();
        return os;
    }

  private:
    void destroyValue() noexcept;

  private:
    TomlType m_type{};
    union
    {
        bool        boolean;  ///< 布尔值
        int64_t     iNumber;  ///< 整数值
        double      dNumber;  ///< 浮点值
        TomlString* string;   ///< 字符串指针
        TomlDate*   date;     ///< 日期指针
        TomlArray*  array;    ///< 数组指针
        TomlObject* object;   ///< 对象指针
    } m_value{};              ///< 存储值的联合体
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
    return parser::Parse({data, length});
}
}  // namespace cctoml

#endif
#pragma clang diagnostic pop
