#include <cctoml.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

using namespace cctoml;

int readToml() {
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
        std::cout << "Successfully parsed config.toml" << '\n';
        // 迭代
        auto test = toml["tbl"];
        for (auto it = test.begin(); it != test.end(); it++) {
            std::cout << "item:\n" << it.key() << ':' << it.value() << '\n';
        }
        std::cout << "----------------------------------------" << '\n';

        // 打印Toml内容
        std::cout << "\nToml Content:" << '\n';
        std::cout << "----------------------------------------" << '\n';
        std::cout << toml.toString(cctoml::parser::TO_JSON, 4) << '\n';
        std::cout << "----------------------------------------" << '\n';

        std::cout << toml << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "parse error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

class TomlConflictDetector {
  private:
    struct Node {
        enum Type { None, Value, ExplictTable, ImplicitTableByValue, ImplicitTableByHeader };
        Type                                         m_type{None};
        std::map<std::string, std::shared_ptr<Node>> m_children;

        explicit Node(Type type) : m_type(type) {}
    };

  public:
    void processKeys(const std::vector<std::string>& keys) {
        auto node = m_root;
        // 中间的
        for (size_t i = 0; !keys.empty() && i < keys.size() - 1; ++i) {
            auto key = getNodeKey(keys[i]);
            if (auto it = node->m_children.find(key); it == node->m_children.end()) {
                // 不存在
                node->m_children.emplace(key, std::make_shared<Node>(Node::ImplicitTableByValue));
            } else {
                // 存在
                auto originType = it->second->m_type;
                if (originType == Node::Value) {
                    // 原本已经是值了
                    throw std::runtime_error("fuck1" + key);
                }
            }
            node = node->m_children[key];
        }
        // 最后一个
        auto key = getNodeKey(keys.back());
        if (auto it = node->m_children.find(key); it == node->m_children.end()) {
            // 不存在
            node->m_children.emplace(key, std::make_shared<Node>(Node::Value));
        } else {
            // 存在
            throw std::runtime_error("fuck2" + key);
        }
    }

    void processTableHeader(const std::vector<std::string>& keys) {
        // 处理创建header
        auto node = m_root;
        for (size_t i = 0; !keys.empty() && i < keys.size() - 1; ++i) {
            auto key = getNodeKey(keys[i]);
            if (auto it = node->m_children.find(key); it == node->m_children.end()) {
                // 不存在
                node->m_children.emplace(key, std::make_shared<Node>(Node::ImplicitTableByHeader));
            } else {
                // 存在
                auto originType = it->second->m_type;
                if (originType == Node::Value) {
                    throw std::runtime_error("fuck3");
                }
            }
            node = node->m_children[key];
        }
        auto key = getNodeKey(keys.back());
        if (auto it = node->m_children.find(keys.back()); it == node->m_children.end()) {
            // 不存在
            node->m_children.emplace(key, std::make_shared<Node>(Node::ExplictTable));
        } else {
            // 存在
            auto originType = it->second->m_type;
            // [a.b.c.d]可以创建[a.b.c]
            // a.b.c.d = 1 不可以创建 [a.b.c]
            if (originType == Node::Value || originType == Node::ExplictTable ||
                originType == Node::ImplicitTableByValue) {
                throw std::runtime_error("fuck4");
            }
        }
    }

    void processArrayHeader(const std::vector<std::string>& keys) {}

    void addSubTomlDetector(const TomlConflictDetector& detector) {
        // 将detector中的内容添加到当前conflict中
    };

  private:
    static inline std::string getNodeKey(const std::string& key) {
        return isBareKey(key) ? key : stringifyKey(key);
    }

    static bool isBareKey(const std::string& s) {
        // 空字符串不是合法的bareKey
        if (s.empty()) {
            return false;
        }
        // 第一个字符不能是数字
        if ('0' <= s.front() && s.front() <= '9') {
            return false;
        }
        // 使用std::all_of检查所有字符是否符合要求
        return std::all_of(s.begin(), s.end(),
                           [](char c) { return std::isalnum(c) || c == '_' || c == '-'; });
    }

    static inline std::string stringifyKey(const std::string& s) {
        return {"\"" + s + "\""};
    }

  private:
    std::shared_ptr<Node> m_root{std::make_shared<Node>(Node::None)};
};

int main() {
    readToml();

    // parseKeyValuePairs创建一个detector传入parseKeyValue,调用processKeys(),这一层是顶层的
    // ->再传入到value中,传入parseObject()或者parseArray()函数
    // 在parseObject内部,我希望解决的是内部key冲突的问题,如{ fruit = { apple.color = "red" },
    // fruit.apple.texture = { smooth = true } } fruit内部和fruit.冲突
    // 如果创建一个detector,则其会检测内部的fruit和fruit.apple.texture

    //    TomlConflictDetector detector;
    //    detector.processKeys({"tbl"});
    //    detector.processKeys({"fruit"});
    //    detector.processKeys({"apple", "color"});
    //    detector.processKeys({"fruit", "apple", "texture"});
    //    detector.processKeys({"smooth"});

    // parseKeyValuePairs()时会解析一个个key-value，需要传入一个detector
    // 在parseObject时需要一个局部的detector->为了解决object内部的问题->这个detector还需要返回->为了解决问题object内部套娃object的问题

    // 如果是table -> [a.b.c]
    // 则设定a.b.c及下属的m.n为true

    // 如果是 a.b.c = {},则a.b可以继续使用
    // [[a.b]]->a.b是array则下面的可以不用管了

    //    Trie tree;
    //    tree.insert("fruit.apple.color", Trie::Object);
    //    tree.insert("fruit.apple.taste.sweet", true);
    //    std::cout << tree.findLongestPrefix("fruit.apple.taste").first << std::endl;
    //    tree.clear();
    //    tree.insert("furi", true);
    return 0;
}