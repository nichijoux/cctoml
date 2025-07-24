#include <cctoml.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>

using namespace cctoml;
// using namespace std;

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
        std::cout << toml.toString() << std::endl;
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

class Trie {
  public:
    enum Type { None, Array, Object };

  private:
    struct TrieNode {
        std::string                                pathSegment;
        Type                                       value;
        std::unordered_map<std::string, TrieNode*> children;

        explicit TrieNode(std::string segment) : pathSegment(std::move(segment)), value(None) {}
    };

    TrieNode* root;

  public:
    Trie() {
        root = new TrieNode("");
    }

    ~Trie() {
        destroyTrie(root);
    }

    void clear() {
        destroyTrie(this->root);
        root = new TrieNode("");
    }

    // 插入键值对
    void insert(const std::string& key, Type value) {
        std::vector<std::string> segments = split(key);
        TrieNode*                node     = root;
        // 遍历子节点
        for (const auto& segment : segments) {
            if (node->children.find(segment) == node->children.end()) {
                // 不存在该segment，则创建
                node->children.emplace(segment, new TrieNode(segment));
            }
            node = node->children[segment];
        }
        node->value = value;
    }

    // 查找键对应的值
    [[nodiscard]] Type find(const std::string& key) const {
        const TrieNode* node = findNode(key);
        return node != nullptr ? node->value : None;
    }

    // 判断是否存在以 prefix 为前缀的键
    [[nodiscard]] bool hasPrefix(const std::string& prefix) const {
        return findNode(prefix) != nullptr;
    }

    // 查找最长有效前缀
    [[nodiscard]] std::pair<std::string, bool> findLongestPrefix(const std::string& key) const {
        std::vector<std::string> segments = split(key);
        const TrieNode*          current  = root;
        std::string              currentPath;
        // 遍历查找
        for (size_t i = 0; i < segments.size(); ++i) {
            auto it = current->children.find(segments[i]);
            if (it == current->children.end()) {
                break;
            }
            current = it->second;
            currentPath += (i == 0 ? "" : ".") + segments[i];
        }
        return {currentPath, current->value};
    }

  private:
    // 按 '.' 分割路径
    [[nodiscard]] static std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> result;
        size_t                   start = 0;
        size_t                   end   = s.find('.');

        while (end != std::string::npos) {
            result.push_back(s.substr(start, end - start));
            start = end + 1;
            end   = s.find('.', start);
        }

        // 处理最后一个子串（或空字符串）
        result.push_back(s.substr(start));

        return result;
    }

    // 查找键对应的节点
    [[nodiscard]] const TrieNode* findNode(const std::string& key) const {
        std::vector<std::string> segments = split(key);
        const TrieNode*          node     = root;

        for (const auto& segment : segments) {
            auto it = node->children.find(segment);
            if (it == node->children.end()) {
                return nullptr;
            }
            node = it->second;
        }
        return node;
    }

    // 销毁前缀树，释放内存
    void destroyTrie(TrieNode* node) {
        if (!node) {
            return;
        }
        for (auto& [k, v] : node->children) {
            destroyTrie(v);
        }
        delete node;
    }
};

int main() {
    readToml();

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