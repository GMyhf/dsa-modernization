#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace dsa::advanced {

// >>> genlist-node
// 广义表结点：标记位区分原子和子表，这正是原书 12.2.1 图 12.7–12.9 画的东西。
// 表用「头 + 尾」表示：非空表 = 表头（第一个元素）+ 表尾（去掉表头后剩下的表）。
//
// 共享（再入表）是本节的真正难点：同一个子表可以挂在多处，所以结点不能按树来递归
// delete——那样会重复释放。这里手写引用计数，因为「谁来回收」就是 12.2 要教的内容；
// 换成 shared_ptr 等于把这一节删掉。计数放在结点上，句柄 GenList 负责加减。
struct GenNode {
    enum class Tag { Atom, List };

    Tag tag = Tag::Atom;
    char value = '\0';   // tag == Atom 时有效
    GenNode* head = nullptr;  // tag == List 时有效：表头
    GenNode* tail = nullptr;  // tag == List 时有效：表尾，空表用 nullptr
    std::size_t refs = 0;
};
// <<< genlist-node

// >>> genlist-handle
class GenList {
public:
    GenList() noexcept = default;  // 空表

    GenList(const GenList& other) noexcept : node_(other.node_) { retain(node_); }

    GenList(GenList&& other) noexcept : node_(other.node_) { other.node_ = nullptr; }

    GenList& operator=(const GenList& other) noexcept {
        if (this != &other) {
            retain(other.node_);   // 先加后减：自赋值和别名都不会先把自己释放掉
            release(node_);
            node_ = other.node_;
        }
        return *this;
    }

    GenList& operator=(GenList&& other) noexcept {
        if (this != &other) {
            release(node_);
            node_ = other.node_;
            other.node_ = nullptr;
        }
        return *this;
    }

    ~GenList() { release(node_); }

    static GenList atom(char value) {
        auto* node = new GenNode{GenNode::Tag::Atom, value, nullptr, nullptr, 0};
        return GenList(node);
    }

    /// 表头 + 表尾 → 新表。原书的「任何非空广义表都能唯一拆成头和尾」的逆运算。
    static GenList cons(const GenList& head, const GenList& tail) {
        if (tail.node_ != nullptr && tail.node_->tag != GenNode::Tag::List) {
            throw std::invalid_argument("tail must be a list");
        }
        auto* node = new GenNode{GenNode::Tag::List, '\0', head.node_, tail.node_, 0};
        retain(node->head);
        retain(node->tail);
        return GenList(node);
    }

    [[nodiscard]] bool is_empty() const noexcept { return node_ == nullptr; }

    [[nodiscard]] bool is_atom() const noexcept {
        return node_ != nullptr && node_->tag == GenNode::Tag::Atom;
    }

    /// 原子的值。非原子是调用方用错了接口，不是「空结果」，所以抛异常。
    [[nodiscard]] char value() const {
        if (!is_atom()) {
            throw std::invalid_argument("not an atom");
        }
        return node_->value;
    }

    /// 空表既没有头也没有尾——这是预期状态，返回 nullopt 而不是抛。
    [[nodiscard]] std::optional<GenList> head() const {
        if (node_ == nullptr || node_->tag != GenNode::Tag::List) {
            return std::nullopt;
        }
        return GenList(node_->head);
    }

    [[nodiscard]] std::optional<GenList> tail() const {
        if (node_ == nullptr || node_->tag != GenNode::Tag::List) {
            return std::nullopt;
        }
        return GenList(node_->tail);
    }

    /// 顶层元素个数：原子算一个，子表也算一个。
    [[nodiscard]] std::size_t length() const noexcept {
        std::size_t count = 0;
        for (const GenNode* cursor = node_;
             cursor != nullptr && cursor->tag == GenNode::Tag::List;
             cursor = cursor->tail) {
            ++count;
        }
        return count;
    }

    /// 表的深度：原子 0，空表 1，其余是「各元素深度的最大值 + 1」。
    [[nodiscard]] std::size_t depth() const noexcept { return depth_of(node_); }

    [[nodiscard]] std::size_t atom_count() const noexcept { return atoms_of(node_); }

    /// 这个子表当前被几个地方引用。教学用：共享一发生，计数就大于 1。
    [[nodiscard]] std::size_t use_count() const noexcept {
        return node_ == nullptr ? 0 : node_->refs;
    }

    [[nodiscard]] std::string to_string() const {
        std::string out;
        write(node_, out);
        return out;
    }

    /// 读 "(a,(b,c),d)" 这种书面形式。原子是单个非括号非逗号字符。
    static GenList parse(std::string_view text) {
        std::size_t pos = 0;
        GenList result = parse_element(text, pos);
        skip_spaces(text, pos);
        if (pos != text.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    }

private:
    explicit GenList(GenNode* node) noexcept : node_(node) { retain(node_); }

// >>> genlist-refcount
    static void retain(GenNode* node) noexcept {
        if (node != nullptr) {
            ++node->refs;
        }
    }

    static void release(GenNode* node) noexcept {
        // 计数归零才真正删除；共享的子表因此只会被删一次。
        while (node != nullptr && --node->refs == 0) {
            GenNode* const head = node->head;
            GenNode* const tail = node->tail;
            delete node;
            // 表尾用循环走，长表不会把栈压穿；表头递归，深度由嵌套层数决定。
            release(head);
            node = tail;
        }
    }
// <<< genlist-refcount

    static std::size_t depth_of(const GenNode* node) noexcept {
        if (node == nullptr) {
            return 1;  // 空表深度 1
        }
        if (node->tag == GenNode::Tag::Atom) {
            return 0;
        }
        std::size_t deepest = 0;
        for (const GenNode* cursor = node; cursor != nullptr; cursor = cursor->tail) {
            const std::size_t d = depth_of(cursor->head);
            if (d > deepest) {
                deepest = d;
            }
        }
        return deepest + 1;
    }

    static std::size_t atoms_of(const GenNode* node) noexcept {
        if (node == nullptr) {
            return 0;
        }
        if (node->tag == GenNode::Tag::Atom) {
            return 1;
        }
        std::size_t total = 0;
        for (const GenNode* cursor = node; cursor != nullptr; cursor = cursor->tail) {
            total += atoms_of(cursor->head);
        }
        return total;
    }

    static void write(const GenNode* node, std::string& out) {
        if (node == nullptr) {
            out += "()";
            return;
        }
        if (node->tag == GenNode::Tag::Atom) {
            out += node->value;
            return;
        }
        out += '(';
        for (const GenNode* cursor = node; cursor != nullptr; cursor = cursor->tail) {
            if (cursor != node) {
                out += ',';
            }
            write(cursor->head, out);
        }
        out += ')';
    }

    static void skip_spaces(std::string_view text, std::size_t& pos) noexcept {
        while (pos < text.size() && text[pos] == ' ') {
            ++pos;
        }
    }

    static GenList parse_element(std::string_view text, std::size_t& pos) {
        skip_spaces(text, pos);
        if (pos >= text.size()) {
            throw std::invalid_argument("unexpected end");
        }
        if (text[pos] != '(') {
            const char value = text[pos];
            if (value == ')' || value == ',') {
                throw std::invalid_argument("expected an atom");
            }
            ++pos;
            return atom(value);
        }
        ++pos;  // 吃掉 '('
        return parse_tail(text, pos);
    }

    /// 读若干元素直到 ')'，把它们连成一个表——正好就是「表尾」的定义。
    static GenList parse_tail(std::string_view text, std::size_t& pos) {
        skip_spaces(text, pos);
        if (pos >= text.size()) {
            throw std::invalid_argument("missing )");
        }
        if (text[pos] == ')') {
            ++pos;
            return GenList();
        }
        GenList first = parse_element(text, pos);
        skip_spaces(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;  // 还有下一个元素
            skip_spaces(text, pos);
            if (pos < text.size() && text[pos] == ')') {
                // "(a,)" 不是「a 后面跟一个空表」，是写漏了——空表要写成 "(a,())"。
                throw std::invalid_argument("trailing comma");
            }
        } else if (pos >= text.size() || text[pos] != ')') {
            throw std::invalid_argument("expected , or )");
        }
        return cons(first, parse_tail(text, pos));
    }

    GenNode* node_ = nullptr;
};
// <<< genlist-handle

}  // namespace dsa::advanced
