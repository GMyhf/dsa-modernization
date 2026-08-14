#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dsa::index {

// >>> bplus-tree
/// B+ 树：记录全在叶上，内部结点只作路标，叶之间有横链。
///
/// 本章约定（与 book/ch11-index.md 11.4 一致，和原书 11.4.2 的另一套约定不同）：
/// - `order` 是**一个结点最多几个孩子**，所以叶和内部结点都最多 `order - 1` 个关键码；
/// - 叶分裂时分界码取右叶的最小关键码，**复写**一份上推（叶上仍保留）；
/// - 内部结点分裂时取中位数**上移**（原结点不再保留）。
///
/// 每个结点当作一个磁盘页：`page_reads()` / `page_writes()` 数的是页访问次数，
/// 这才是外存结构的关键指标——不是 CPU 比较次数。
class BPlusTree {
public:
    explicit BPlusTree(std::size_t order) : order_(order) {
        if (order < 3) {
            throw std::invalid_argument("order must be >= 3");
        }
        root_ = std::make_unique<Node>(true);
        first_leaf_ = root_.get();
    }

    /// 批量装入：按页填满，自底向上建内部层。这就是 11.2 静态多分树的建法——
    /// 一次建好、查找快；后续插入删除仍然按 B+ 树的分裂合并走，不需要重组整棵树。
    /// `per_leaf` 是每个叶装多少个关键码，必须在下限和上限之间。
    static BPlusTree bulk_load(std::size_t order,
                               const std::vector<std::pair<int, std::string>>& sorted,
                               std::size_t per_leaf) {
        BPlusTree tree(order);
        if (per_leaf < tree.min_keys(true) || per_leaf > tree.max_keys()) {
            throw std::invalid_argument("per_leaf out of range");
        }
        for (std::size_t i = 1; i < sorted.size(); ++i) {
            if (sorted[i - 1].first >= sorted[i].first) {
                throw std::invalid_argument("bulk_load needs strictly increasing keys");
            }
        }
        if (sorted.empty()) {
            return tree;
        }

        std::vector<std::unique_ptr<Node>> level;
        for (std::size_t i = 0; i < sorted.size(); i += per_leaf) {
            auto leaf = std::make_unique<Node>(true);
            for (std::size_t j = i; j < sorted.size() && j < i + per_leaf; ++j) {
                leaf->keys.push_back(sorted[j].first);
                leaf->values.push_back(sorted[j].second);
            }
            if (!level.empty()) {
                level.back()->next = leaf.get();
            }
            level.push_back(std::move(leaf));
        }
        rebalance_last_page(level, tree.min_keys(true));
        tree.first_leaf_ = level.front().get();
        tree.size_ = sorted.size();

        while (level.size() > 1) {
            std::vector<std::unique_ptr<Node>> parents;
            for (std::size_t i = 0; i < level.size(); i += order) {
                auto parent = std::make_unique<Node>(false);
                for (std::size_t j = i; j < level.size() && j < i + order; ++j) {
                    if (j != i) {
                        parent->keys.push_back(smallest_key(level[j].get()));
                    }
                    parent->children.push_back(std::move(level[j]));
                }
                parents.push_back(std::move(parent));
            }
            rebalance_last_page(parents, tree.min_keys(false));
            level = std::move(parents);
        }
        tree.root_ = std::move(level.front());
        return tree;
    }

    /// 插入或覆盖。返回 true 表示新增，false 表示覆盖了已有关键码。
    bool insert(int key, std::string value) {
        Split split;
        const bool added = insert_into(root_.get(), key, std::move(value), split);
        if (split.happened) {
            // 根裂了，树长高一层。
            auto new_root = std::make_unique<Node>(false);
            new_root->keys.push_back(split.separator);
            new_root->children.push_back(std::move(root_));
            new_root->children.push_back(std::move(split.right));
            root_ = std::move(new_root);
            ++writes_;
        }
        if (added) {
            ++size_;
        }
        return added;
    }

    [[nodiscard]] std::optional<std::string> find(int key) const {
        const Node* node = root_.get();
        while (!node->leaf) {
            ++reads_;
            node = node->children[child_slot(*node, key)].get();
        }
        ++reads_;
        const auto at = std::lower_bound(node->keys.begin(), node->keys.end(), key);
        if (at == node->keys.end() || *at != key) {
            return std::nullopt;  // 查不到是预期状态，不是错误
        }
        return node->values[static_cast<std::size_t>(at - node->keys.begin())];
    }

// >>> bplus-range
    /// 范围扫描：找到下限所在的叶，然后沿叶链横着走，不再回到内部结点。
    [[nodiscard]] std::vector<std::pair<int, std::string>> range(int low, int high) const {
        std::vector<std::pair<int, std::string>> out;
        if (low > high) {
            return out;
        }
        const Node* node = root_.get();
        while (!node->leaf) {
            ++reads_;
            node = node->children[child_slot(*node, low)].get();
        }
        while (node != nullptr) {
            ++reads_;
            for (std::size_t i = 0; i < node->keys.size(); ++i) {
                if (node->keys[i] > high) {
                    return out;
                }
                if (node->keys[i] >= low) {
                    out.emplace_back(node->keys[i], node->values[i]);
                }
            }
            node = node->next;
        }
        return out;
    }
// <<< bplus-range

    bool erase(int key) {
        if (!erase_from(root_.get(), key)) {
            return false;
        }
        --size_;
        // 根只剩一个孩子、自己没有关键码了：树矮一层。
        if (!root_->leaf && root_->keys.empty()) {
            root_ = std::move(root_->children[0]);
            ++writes_;
        }
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] std::size_t height() const noexcept {
        std::size_t levels = 1;
        for (const Node* node = root_.get(); !node->leaf; node = node->children[0].get()) {
            ++levels;
        }
        return levels;
    }

    [[nodiscard]] std::size_t leaf_count() const noexcept {
        std::size_t count = 0;
        for (const Node* leaf = first_leaf_; leaf != nullptr; leaf = leaf->next) {
            ++count;
        }
        return count;
    }

    /// 逐层打印，方便把测试写成原书那样的树形断言：
    /// `[50] / [30] [70] / [10,20] [30] [50,60] [70,90]`
    [[nodiscard]] std::string to_string() const {
        std::string out;
        std::vector<const Node*> level{root_.get()};
        while (!level.empty()) {
            if (!out.empty()) {
                out += " / ";
            }
            std::vector<const Node*> next;
            for (const Node* node : level) {
                out += '[';
                for (std::size_t i = 0; i < node->keys.size(); ++i) {
                    if (i != 0) {
                        out += ',';
                    }
                    out += std::to_string(node->keys[i]);
                }
                out += ']';
                if (node != level.back()) {
                    out += ' ';
                }
                for (const auto& child : node->children) {
                    next.push_back(child.get());
                }
            }
            level = std::move(next);
        }
        return out;
    }

    /// 结构不变量：所有叶同层、结点不超上限、除根外不低于下限、分界码与子树一致、
    /// 叶链顺序与叶内顺序整体有序。测试靠它在随机操作后判定树没坏。
    [[nodiscard]] bool validate() const {
        std::size_t leaf_depth = 0;
        if (!check_node(root_.get(), 0, leaf_depth, true, std::nullopt, std::nullopt)) {
            return false;
        }
        // 叶链必须整体升序，且总数与 size() 一致。
        std::size_t counted = 0;
        std::optional<int> previous;
        for (const Node* leaf = first_leaf_; leaf != nullptr; leaf = leaf->next) {
            for (const int key : leaf->keys) {
                if (previous && *previous >= key) {
                    return false;
                }
                previous = key;
                ++counted;
            }
        }
        return counted == size_;
    }

    [[nodiscard]] std::size_t page_reads() const noexcept { return reads_; }
    [[nodiscard]] std::size_t page_writes() const noexcept { return writes_; }
    void reset_counters() noexcept {
        reads_ = 0;
        writes_ = 0;
    }

private:
    struct Node {
        explicit Node(bool is_leaf) : leaf(is_leaf) {}

        bool leaf;
        std::vector<int> keys;
        std::vector<std::string> values;               // 仅叶
        std::vector<std::unique_ptr<Node>> children;   // 仅内部结点
        Node* next = nullptr;                          // 仅叶：横链
    };

    struct Split {
        bool happened = false;
        int separator = 0;
        std::unique_ptr<Node> right;
    };

    [[nodiscard]] std::size_t max_keys() const noexcept { return order_ - 1; }

    /// 除根之外的下限：内部结点至少 ⌈order/2⌉ 个孩子，叶至少 ⌈(order-1)/2⌉ 个关键码。
    [[nodiscard]] std::size_t min_keys(bool leaf) const noexcept {
        return leaf ? (order_ - 1 + 1) / 2 : (order_ + 1) / 2 - 1;
    }

    static std::size_t child_slot(const Node& node, int key) noexcept {
        // 分界码是右子树的最小关键码，所以等于分界码要走右边。
        const auto at = std::upper_bound(node.keys.begin(), node.keys.end(), key);
        return static_cast<std::size_t>(at - node.keys.begin());
    }

    bool insert_into(Node* node, int key, std::string value, Split& split) {
        if (node->leaf) {
            ++reads_;
            const auto at = std::lower_bound(node->keys.begin(), node->keys.end(), key);
            const auto pos = static_cast<std::size_t>(at - node->keys.begin());
            if (at != node->keys.end() && *at == key) {
                node->values[pos] = std::move(value);
                ++writes_;
                return false;
            }
            node->keys.insert(at, key);
            node->values.insert(node->values.begin() + static_cast<std::ptrdiff_t>(pos), std::move(value));
            ++writes_;
            if (node->keys.size() > max_keys()) {
                split_leaf(node, split);
            }
            return true;
        }

        ++reads_;
        const std::size_t slot = child_slot(*node, key);
        Split child_split;
        const bool added =
            insert_into(node->children[slot].get(), key, std::move(value), child_split);
        if (child_split.happened) {
            node->keys.insert(node->keys.begin() + static_cast<std::ptrdiff_t>(slot),
                              child_split.separator);
            node->children.insert(node->children.begin() + static_cast<std::ptrdiff_t>(slot) + 1,
                                  std::move(child_split.right));
            ++writes_;
            if (node->keys.size() > max_keys()) {
                split_internal(node, split);
            }
        }
        return added;
    }

// >>> bplus-split
    void split_leaf(Node* node, Split& split) {
        const std::size_t mid = node->keys.size() / 2;
        auto right = std::make_unique<Node>(true);
        right->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(mid), node->keys.end());
        right->values.assign(std::make_move_iterator(node->values.begin() + static_cast<std::ptrdiff_t>(mid)),
                             std::make_move_iterator(node->values.end()));
        node->keys.resize(mid);
        node->values.resize(mid);
        right->next = node->next;
        node->next = right.get();
        // 叶分裂：分界码是右叶最小关键码，复写上推，叶上仍保留。
        split.happened = true;
        split.separator = right->keys.front();
        split.right = std::move(right);
        writes_ += 2;
    }

    void split_internal(Node* node, Split& split) {
        const std::size_t mid = node->keys.size() / 2;
        auto right = std::make_unique<Node>(false);
        // 内部结点分裂：中位数上移，原结点不再保留它。
        split.separator = node->keys[mid];
        right->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(mid) + 1, node->keys.end());
        right->children.assign(
            std::make_move_iterator(node->children.begin() + static_cast<std::ptrdiff_t>(mid) + 1),
            std::make_move_iterator(node->children.end()));
        node->keys.resize(mid);
        node->children.resize(mid + 1);
        split.happened = true;
        split.right = std::move(right);
        writes_ += 2;
    }
// <<< bplus-split

    bool erase_from(Node* node, int key) {
        if (node->leaf) {
            ++reads_;
            const auto at = std::lower_bound(node->keys.begin(), node->keys.end(), key);
            if (at == node->keys.end() || *at != key) {
                return false;
            }
            const auto pos = static_cast<std::size_t>(at - node->keys.begin());
            node->keys.erase(at);
            node->values.erase(node->values.begin() + static_cast<std::ptrdiff_t>(pos));
            ++writes_;
            return true;
        }

        ++reads_;
        const std::size_t slot = child_slot(*node, key);
        if (!erase_from(node->children[slot].get(), key)) {
            return false;
        }
        if (node->children[slot]->keys.size() < min_keys(node->children[slot]->leaf)) {
            rebalance(node, slot);
            // 借位和合并都会把父结点的分界码搬进孩子里，而那个分界码可能正是刚被删掉的
            // 关键码（合并时尤其明显）。搬完必须让孩子按「分界码 = 右子树最小关键码」重算，
            // 否则树里会留下一个指向已删关键码的路标。
            for (auto& child : node->children) {
                if (!child->leaf) {
                    refresh_separators(child.get());
                }
            }
        }
        // 本层的分界码同样可能过期（例如删掉的正是某棵子树的最小关键码）。
        refresh_separators(node);
        return true;
    }

    /// 下溢处理：先向左右兄弟借一个，借不到就合并。两者都可能继续向上传播。
    void rebalance(Node* parent, std::size_t slot) {
        Node* child = parent->children[slot].get();
        if (slot > 0) {
            Node* left = parent->children[slot - 1].get();
            if (left->keys.size() > min_keys(left->leaf)) {
                borrow_from_left(parent, slot, left, child);
                return;
            }
        }
        if (slot + 1 < parent->children.size()) {
            Node* right = parent->children[slot + 1].get();
            if (right->keys.size() > min_keys(right->leaf)) {
                borrow_from_right(parent, slot, child, right);
                return;
            }
        }
        if (slot > 0) {
            merge(parent, slot - 1);
        } else {
            merge(parent, slot);
        }
    }

    void borrow_from_left(Node* parent, std::size_t slot, Node* left, Node* child) {
        if (child->leaf) {
            child->keys.insert(child->keys.begin(), left->keys.back());
            child->values.insert(child->values.begin(), std::move(left->values.back()));
            left->keys.pop_back();
            left->values.pop_back();
        } else {
            // 内部结点借位要走父结点：父的分界码下来，兄弟的末位上去。
            child->keys.insert(child->keys.begin(), parent->keys[slot - 1]);
            parent->keys[slot - 1] = left->keys.back();
            left->keys.pop_back();
            child->children.insert(child->children.begin(), std::move(left->children.back()));
            left->children.pop_back();
        }
        writes_ += 3;
    }

    void borrow_from_right(Node* parent, std::size_t slot, Node* child, Node* right) {
        if (child->leaf) {
            child->keys.push_back(right->keys.front());
            child->values.push_back(std::move(right->values.front()));
            right->keys.erase(right->keys.begin());
            right->values.erase(right->values.begin());
        } else {
            child->keys.push_back(parent->keys[slot]);
            parent->keys[slot] = right->keys.front();
            right->keys.erase(right->keys.begin());
            child->children.push_back(std::move(right->children.front()));
            right->children.erase(right->children.begin());
        }
        writes_ += 3;
    }

    /// 把 children[slot] 和 children[slot+1] 合成一个，父结点少一个分界码。
    void merge(Node* parent, std::size_t slot) {
        Node* left = parent->children[slot].get();
        std::unique_ptr<Node> right = std::move(parent->children[slot + 1]);
        if (left->leaf) {
            left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
            left->values.insert(left->values.end(),
                                std::make_move_iterator(right->values.begin()),
                                std::make_move_iterator(right->values.end()));
            left->next = right->next;  // 叶链要接上，否则范围扫描会断
        } else {
            // 内部结点合并：父的分界码沉下来，重新成为一个普通关键码。
            left->keys.push_back(parent->keys[slot]);
            left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
            for (auto& child : right->children) {
                left->children.push_back(std::move(child));
            }
        }
        parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(slot));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(slot) + 1);
        writes_ += 2;
    }

    /// 分界码 = 右子树的最小关键码。借位、合并、删除最小值之后都要重算。
    void refresh_separators(Node* node) {
        for (std::size_t i = 0; i < node->keys.size(); ++i) {
            node->keys[i] = smallest_key(node->children[i + 1].get());
        }
    }

    /// 批量装入时最后一页可能装不满。从前一页匀一点过来，别让它低于下限。
    static void rebalance_last_page(std::vector<std::unique_ptr<Node>>& level, std::size_t min) {
        if (level.size() < 2) {
            return;  // 只有一个结点时它就是根，根不受下限约束
        }
        Node* last = level.back().get();
        Node* prev = level[level.size() - 2].get();
        while (last->keys.size() < min) {
            if (last->leaf) {
                if (prev->keys.size() <= min) {
                    break;
                }
                last->keys.insert(last->keys.begin(), prev->keys.back());
                last->values.insert(last->values.begin(), std::move(prev->values.back()));
                prev->keys.pop_back();
                prev->values.pop_back();
            } else {
                if (prev->children.size() <= min + 1) {
                    break;
                }
                last->children.insert(last->children.begin(), std::move(prev->children.back()));
                prev->children.pop_back();
                prev->keys.pop_back();
                last->keys.clear();
                for (std::size_t i = 1; i < last->children.size(); ++i) {
                    last->keys.push_back(smallest_key(last->children[i].get()));
                }
            }
        }
    }

    static int smallest_key(const Node* node) {
        while (!node->leaf) {
            node = node->children[0].get();
        }
        return node->keys.front();
    }

    bool check_node(const Node* node, std::size_t depth, std::size_t& leaf_depth, bool is_root,
                    std::optional<int> low, std::optional<int> high) const {
        if (!std::is_sorted(node->keys.begin(), node->keys.end())) {
            return false;
        }
        if (node->keys.size() > max_keys()) {
            return false;
        }
        if (!is_root && node->keys.size() < min_keys(node->leaf)) {
            return false;
        }
        for (const int key : node->keys) {
            if ((low && key < *low) || (high && key >= *high)) {
                return false;
            }
        }
        if (node->leaf) {
            if (node->values.size() != node->keys.size()) {
                return false;
            }
            if (leaf_depth == 0) {
                leaf_depth = depth;
            }
            return leaf_depth == depth;  // 所有叶必须同层
        }
        if (node->children.size() != node->keys.size() + 1) {
            return false;
        }
        for (std::size_t i = 0; i < node->children.size(); ++i) {
            const std::optional<int> child_low = i == 0 ? low : std::optional<int>(node->keys[i - 1]);
            const std::optional<int> child_high =
                i == node->keys.size() ? high : std::optional<int>(node->keys[i]);
            if (!check_node(node->children[i].get(), depth + 1, leaf_depth, false, child_low,
                            child_high)) {
                return false;
            }
            // 分界码必须等于右子树的最小关键码
            if (i > 0 && smallest_key(node->children[i].get()) != node->keys[i - 1]) {
                return false;
            }
        }
        return true;
    }

    std::size_t order_;
    std::unique_ptr<Node> root_;
    Node* first_leaf_ = nullptr;
    std::size_t size_ = 0;
    mutable std::size_t reads_ = 0;
    mutable std::size_t writes_ = 0;
};
// <<< bplus-tree

}  // namespace dsa::index
