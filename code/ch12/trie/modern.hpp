#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dsa::advanced {

// >>> trie
/// 字母 Trie：第 i 层按关键码的第 i 个字符分支，公共前缀在树里只存一次。
/// 字母表限定为 'a'..'z'——原书 12.3 的例子（can/car/cat/do）就在这个字母表里；
/// 越界字符是调用方用错了接口，抛异常而不是悄悄忽略。
class Trie {
public:
    static constexpr std::size_t kAlphabet = 26;

    /// 插入。返回 true 表示这是一个新词；重复插入返回 false。
    bool insert(std::string_view word) {
        validate(word);
        Node* node = &root_;
        for (const char letter : word) {
            const std::size_t slot = index_of(letter);
            if (!node->children[slot]) {
                node->children[slot] = std::make_unique<Node>();
                ++nodes_;
            }
            node = node->children[slot].get();
            ++node->passing;
        }
        if (node->terminal) {
            // 词已存在：把刚才一路加上的 passing 退回去。
            unwind(word);
            return false;
        }
        node->terminal = true;
        ++size_;
        return true;
    }

    [[nodiscard]] bool contains(std::string_view word) const {
        const Node* node = find(word);
        return node != nullptr && node->terminal;
    }

    /// 表里有没有以 prefix 开头的词。这是 Trie 相对 BST 的关键能力。
    [[nodiscard]] bool starts_with(std::string_view prefix) const {
        return find(prefix) != nullptr;
    }

    [[nodiscard]] std::size_t count_with_prefix(std::string_view prefix) const {
        const Node* node = find(prefix);
        if (node == nullptr) {
            return 0;
        }
        return prefix.empty() ? size_ : node->passing;
    }

// >>> trie-longest-prefix
    /// 最长前缀匹配：走到走不动为止，回退到最近的词尾。IP 路由查表就是这个动作。
    [[nodiscard]] std::string longest_prefix_of(std::string_view text) const {
        const Node* node = &root_;
        std::size_t best = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (!is_letter(text[i])) {
                break;
            }
            const Node* next = node->children[index_of(text[i])].get();
            if (next == nullptr) {
                break;
            }
            node = next;
            if (node->terminal) {
                best = i + 1;
            }
        }
        return std::string(text.substr(0, best));
    }
// <<< trie-longest-prefix

    bool erase(std::string_view word) {
        validate(word);
        Node* node = find_mutable(word);
        if (node == nullptr || !node->terminal) {
            return false;
        }
        node->terminal = false;
        --size_;
        unwind(word);
        prune(&root_, word, 0);
        return true;
    }

    [[nodiscard]] std::vector<std::string> keys_with_prefix(std::string_view prefix) const {
        std::vector<std::string> out;
        const Node* node = find(prefix);
        if (node == nullptr) {
            return out;
        }
        std::string buffer(prefix);
        collect(node, buffer, out);
        return out;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// 结点总数（不含根）。拿它和「所有词的总长度」比，就能看出前缀共享省了多少。
    [[nodiscard]] std::size_t node_count() const noexcept { return nodes_; }

private:
    struct Node {
        std::array<std::unique_ptr<Node>, kAlphabet> children{};
        bool terminal = false;
        std::size_t passing = 0;  // 有多少个词经过这个结点
    };

    static bool is_letter(char c) noexcept { return c >= 'a' && c <= 'z'; }

    static std::size_t index_of(char c) noexcept {
        return static_cast<std::size_t>(c - 'a');
    }

    static void validate(std::string_view word) {
        for (const char c : word) {
            if (!is_letter(c)) {
                throw std::invalid_argument("trie key must be a..z");
            }
        }
    }

    void unwind(std::string_view word) {
        Node* node = &root_;
        for (const char letter : word) {
            node = node->children[index_of(letter)].get();
            --node->passing;
        }
    }

    [[nodiscard]] const Node* find(std::string_view key) const {
        const Node* node = &root_;
        for (const char letter : key) {
            if (!is_letter(letter)) {
                return nullptr;
            }
            node = node->children[index_of(letter)].get();
            if (node == nullptr) {
                return nullptr;
            }
        }
        return node;
    }

    Node* find_mutable(std::string_view key) {
        return const_cast<Node*>(find(key));
    }

    /// 删词之后，把不再承载任何词的结点摘掉——否则删多了 Trie 只增不减。
    bool prune(Node* node, std::string_view word, std::size_t depth) {
        if (depth < word.size()) {
            const std::size_t slot = index_of(word[depth]);
            Node* child = node->children[slot].get();
            if (child != nullptr && prune(child, word, depth + 1)) {
                node->children[slot].reset();
                --nodes_;
            }
        }
        if (node == &root_) {
            return false;
        }
        if (node->terminal) {
            return false;
        }
        for (const auto& child : node->children) {
            if (child) {
                return false;
            }
        }
        return true;
    }

    static void collect(const Node* node, std::string& buffer,
                        std::vector<std::string>& out) {
        if (node->terminal) {
            out.push_back(buffer);
        }
        for (std::size_t slot = 0; slot < kAlphabet; ++slot) {
            if (node->children[slot]) {
                buffer.push_back(static_cast<char>('a' + slot));
                collect(node->children[slot].get(), buffer, out);
                buffer.pop_back();
            }
        }
    }

    Node root_;
    std::size_t size_ = 0;
    std::size_t nodes_ = 0;
};
// <<< trie

// >>> patricia
/// Patricia 树：把 Trie 里「只有一个孩子」的结点压缩掉，内部结点只记「跳过几位再比」。
/// 关键码按字节的位串看待，最高位在前；越过关键码长度的位一律读作 0，所以键里不能有 '\0'。
/// 查找沿位下降到一个叶，再和叶上的完整关键码比一次——这一次比较不能省，
/// 因为路上只看了少数几位。
class PatriciaTree {
public:
    PatriciaTree() = default;
    PatriciaTree(const PatriciaTree&) = delete;
    PatriciaTree& operator=(const PatriciaTree&) = delete;
    PatriciaTree(PatriciaTree&&) noexcept = default;
    PatriciaTree& operator=(PatriciaTree&&) noexcept = default;
    ~PatriciaTree() = default;

    bool insert(std::string_view key) {
        validate(key);
        if (root_ == nullptr) {
            root_ = make_leaf(key);
            ++size_;
            return true;
        }
        const Node* leaf = descend(key);
        const optional_bit differing = first_differing_bit(leaf->key, key);
        if (!differing.found) {
            return false;  // 已经在树里
        }

        // 在第 differing.index 位上插入一个新的内部结点。
        std::unique_ptr<Node>* slot = &root_;
        while ((*slot)->is_internal() && (*slot)->bit < differing.index) {
            slot = bit_of(key, (*slot)->bit) ? &(*slot)->right : &(*slot)->left;
        }
        auto node = std::make_unique<Node>();
        node->bit = differing.index;
        auto leaf_node = make_leaf(key);
        if (bit_of(key, differing.index)) {
            node->left = std::move(*slot);
            node->right = std::move(leaf_node);
        } else {
            node->right = std::move(*slot);
            node->left = std::move(leaf_node);
        }
        *slot = std::move(node);
        ++size_;
        ++internal_;
        return true;
    }

    [[nodiscard]] bool contains(std::string_view key) const {
        if (root_ == nullptr) {
            return false;
        }
        validate(key);
        const Node* leaf = descend(key);
        return leaf->key == key;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// 内部结点数。同样一组关键码，它明显少于纯 Trie 的结点数——这就是压缩的效果。
    [[nodiscard]] std::size_t internal_count() const noexcept { return internal_; }

    /// 一次查找要检查的位数（= 从根到叶的内部结点数），教学用。
    [[nodiscard]] std::size_t probe_depth(std::string_view key) const {
        std::size_t depth = 0;
        const Node* node = root_.get();
        while (node != nullptr && node->is_internal()) {
            node = bit_of(key, node->bit) ? node->right.get() : node->left.get();
            ++depth;
        }
        return depth;
    }

private:
    struct Node {
        std::size_t bit = 0;  // 内部结点：比第几位
        std::string key;      // 叶：完整关键码
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        [[nodiscard]] bool is_internal() const noexcept {
            return left != nullptr || right != nullptr;
        }
    };

    struct optional_bit {
        bool found = false;
        std::size_t index = 0;
    };

    static void validate(std::string_view key) {
        for (const char c : key) {
            if (c == '\0') {
                throw std::invalid_argument("patricia key must not contain NUL");
            }
        }
    }

// >>> patricia-bits
    static bool bit_of(std::string_view key, std::size_t index) noexcept {
        const std::size_t byte = index / 8;
        if (byte >= key.size()) {
            return false;  // 越过关键码长度，一律读 0
        }
        const auto value = static_cast<unsigned char>(key[byte]);
        return ((value >> (7 - index % 8)) & 1U) != 0;
    }

    static optional_bit first_differing_bit(std::string_view a, std::string_view b) {
        const std::size_t longest = a.size() > b.size() ? a.size() : b.size();
        const std::size_t bits = (longest + 1) * 8;  // +1 让「一个是另一个的前缀」也能分开
        for (std::size_t i = 0; i < bits; ++i) {
            if (bit_of(a, i) != bit_of(b, i)) {
                return {true, i};
            }
        }
        return {};
    }
// <<< patricia-bits

    static std::unique_ptr<Node> make_leaf(std::string_view key) {
        auto leaf = std::make_unique<Node>();
        leaf->key = std::string(key);
        return leaf;
    }

    [[nodiscard]] const Node* descend(std::string_view key) const {
        const Node* node = root_.get();
        while (node->is_internal()) {
            node = bit_of(key, node->bit) ? node->right.get() : node->left.get();
        }
        return node;
    }

    std::unique_ptr<Node> root_;
    std::size_t size_ = 0;
    std::size_t internal_ = 0;
};
// <<< patricia

}  // namespace dsa::advanced
