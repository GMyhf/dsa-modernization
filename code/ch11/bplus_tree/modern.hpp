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
/// 按原书 11.4.2 的定义（与 book/ch11-index.md 11.4.2 一致）：
/// - `order`（即 m）是**一个结点最多几个孩子**；除根外至少 ⌈m/2⌉ 个孩子，根至少 2 个；
/// - 叶存 ⌈m/2⌉ 到 m 个关键码（树只有一个叶时它就是根，不受下限约束）；
///   混合型 B+ 树（原书图 11.12）的叶另有自己的阶 L，叶存 ⌈L/2⌉ 到 L 个；
/// - 有 k 个孩子的内部结点**有 k 个关键码**，第 i 个是第 i 棵子树**最大关键码的复写**；
/// - 叶和内部结点放进第 m+1 项时分裂，左半 ⌈(m+1)/2⌉ 项、右半其余；两半各自的最大码
///   复写到父结点——分裂只有「复写」，没有「上移」。
///
/// 每个结点当作一个磁盘页：`page_reads()` / `page_writes()` 数的是页访问次数，
/// 这才是外存结构的关键指标——不是 CPU 比较次数。一次插入或删除里，同一页读过就留在
/// 缓冲区、改过只在最后写回一次，这和原书图 11.10、11.11 数读写盘的口径相同。
class BPlusTree {
public:
    using Row = std::pair<int, std::string>;

    explicit BPlusTree(std::size_t order) : BPlusTree(order, order) {}

    /// 混合型 B+ 树：内部结点的阶 `order`，叶的阶 `leaf_order`（叶最多放几个关键码）。
    BPlusTree(std::size_t order, std::size_t leaf_order) : order_(order), leaf_order_(leaf_order) {
        if (order < 3) {
            throw std::invalid_argument("order must be >= 3");
        }
        if (leaf_order < 2) {
            throw std::invalid_argument("leaf_order must be >= 2");
        }
        root_ = std::make_unique<Node>(true);
        first_leaf_ = root_.get();
    }

    /// 按给定的页布局装出一棵树：`leaves` 自左向右是各叶的内容，`fanouts[l]` 是自底向上
    /// 第 l 层内部结点各有几个孩子。用来把书上的图原样搬进程序；装完跑 `validate()`，
    /// 不是一棵合法的 B+ 树就抛 `std::invalid_argument`。
    static BPlusTree assemble(std::size_t order, const std::vector<std::vector<Row>>& leaves,
                              const std::vector<std::vector<std::size_t>>& fanouts) {
        return assemble(order, order, leaves, fanouts);
    }

    static BPlusTree assemble(std::size_t order, std::size_t leaf_order,
                              const std::vector<std::vector<Row>>& leaves,
                              const std::vector<std::vector<std::size_t>>& fanouts) {
        BPlusTree tree(order, leaf_order);
        if (leaves.empty()) {
            if (!fanouts.empty()) {
                throw std::invalid_argument("fanouts given for an empty tree");
            }
            return tree;
        }
        std::vector<std::unique_ptr<Node>> level;
        for (const auto& rows : leaves) {
            auto leaf = std::make_unique<Node>(true);
            for (const auto& row : rows) {
                leaf->keys.push_back(row.first);
                leaf->values.push_back(row.second);
            }
            if (leaf->keys.empty()) {
                throw std::invalid_argument("empty leaf");
            }
            if (!level.empty()) {
                level.back()->next = leaf.get();
            }
            tree.size_ += rows.size();
            level.push_back(std::move(leaf));
        }
        tree.first_leaf_ = level.front().get();
        for (const auto& counts : fanouts) {
            std::vector<std::unique_ptr<Node>> parents;
            std::size_t next = 0;
            for (const std::size_t count : counts) {
                if (count == 0 || next + count > level.size()) {
                    throw std::invalid_argument("fanouts do not match the level below");
                }
                auto parent = std::make_unique<Node>(false);
                for (std::size_t j = next; j < next + count; ++j) {
                    parent->keys.push_back(level[j]->keys.back());  // 最大码复写
                    parent->children.push_back(std::move(level[j]));
                }
                next += count;
                parents.push_back(std::move(parent));
            }
            if (next != level.size()) {
                throw std::invalid_argument("fanouts do not match the level below");
            }
            level = std::move(parents);
        }
        if (level.size() != 1) {
            throw std::invalid_argument("top level must be a single root");
        }
        tree.root_ = std::move(level.front());
        if (!tree.validate()) {
            throw std::invalid_argument("layout is not a valid B+ tree of this order");
        }
        return tree;
    }

    /// 批量装入：按页填满，自底向上建内部层。这就是 11.2 静态多分树的建法——
    /// 一次建好、查找快；后续插入删除仍然按 B+ 树的分裂合并走，不需要重组整棵树。
    /// `per_leaf` 是每个叶装多少个关键码，必须在下限和上限之间；最后一页不够下限时
    /// 从前一页匀过来，匀不开就并进前一页。
    static BPlusTree bulk_load(std::size_t order, const std::vector<Row>& sorted,
                               std::size_t per_leaf) {
        const std::size_t min_fill = (order + 1) / 2;
        if (order < 3) {
            throw std::invalid_argument("order must be >= 3");
        }
        if (per_leaf < min_fill || per_leaf > order) {
            throw std::invalid_argument("per_leaf out of range");
        }
        for (std::size_t i = 1; i < sorted.size(); ++i) {
            if (sorted[i - 1].first >= sorted[i].first) {
                throw std::invalid_argument("bulk_load needs strictly increasing keys");
            }
        }
        std::vector<std::vector<Row>> leaves;
        std::size_t start = 0;
        for (const std::size_t count : page_sizes(sorted.size(), per_leaf, min_fill)) {
            leaves.emplace_back(sorted.begin() + static_cast<std::ptrdiff_t>(start),
                                sorted.begin() + static_cast<std::ptrdiff_t>(start + count));
            start += count;
        }
        std::vector<std::vector<std::size_t>> fanouts;
        for (std::size_t nodes = leaves.size(); nodes > 1;) {
            fanouts.push_back(page_sizes(nodes, order, min_fill));
            nodes = fanouts.back().size();
        }
        return assemble(order, leaves, fanouts);
    }

    /// 插入或覆盖。返回 true 表示新增，false 表示覆盖了已有关键码。
    bool insert(int key, std::string value) {
        begin_op();
        Split split;
        const bool added = insert_into(root_.get(), key, std::move(value), split);
        if (split.happened) {
            // 根裂了，树长高一层：新根的两个关键码就是两半各自的最大码。
            auto new_root = std::make_unique<Node>(false);
            new_root->keys.push_back(root_->keys.back());
            new_root->keys.push_back(split.right->keys.back());
            new_root->children.push_back(std::move(root_));
            new_root->children.push_back(std::move(split.right));
            root_ = std::move(new_root);
            mark_dirty(root_.get());
        }
        if (added) {
            ++size_;
        }
        end_op();
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
    [[nodiscard]] std::vector<Row> range(int low, int high) const {
        std::vector<Row> out;
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
        begin_op();
        const bool removed = erase_from(root_.get(), key);
        if (removed) {
            --size_;
            // 根只剩一个孩子：删去根、让孩子当新根，树矮一层（根至少要有两个孩子）。
            if (!root_->leaf && root_->children.size() == 1) {
                forget(root_.get());
                root_ = std::move(root_->children[0]);
            }
        }
        end_op();
        return removed;
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

    /// 逐层打印，方便把测试写成原书那样的树形断言（图 11.10 的上两层）：
    /// `[40,90] / [20,40] [70,90] / ...`
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

    /// 结构不变量：所有叶同层；结点不超上限、除根外不低于下限、根若是内部结点至少两个孩子；
    /// 内部结点关键码数等于孩子数，第 i 个关键码不小于第 i 棵子树的全部关键码、
    /// 小于第 i+1 棵子树的全部关键码；叶链整体升序且总数等于 `size()`。
    [[nodiscard]] bool validate() const {
        std::size_t leaf_depth = 0;
        if (!check_node(root_.get(), 1, leaf_depth, true, std::nullopt, std::nullopt)) {
            return false;
        }
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
        std::vector<int> keys;                         // 叶：关键码；内部：各子树最大码
        std::vector<std::string> values;               // 仅叶
        std::vector<std::unique_ptr<Node>> children;   // 仅内部结点，与 keys 一一对应
        Node* next = nullptr;                          // 仅叶：横链
    };

    struct Split {
        bool happened = false;
        std::unique_ptr<Node> right;
    };

    /// 一个结点有几项：叶数关键码，内部结点数孩子（两者在本定义下数目相同）。
    static std::size_t entries(const Node& node) noexcept { return node.keys.size(); }

    /// 上限：叶至多 L 个关键码，内部结点至多 m 个孩子。
    [[nodiscard]] std::size_t max_entries(const Node& node) const noexcept {
        return node.leaf ? leaf_order_ : order_;
    }

    /// 除根外的下限：上限的一半向上取整。
    [[nodiscard]] std::size_t min_entries(const Node& node) const noexcept {
        return (max_entries(node) + 1) / 2;
    }

    /// 第一个不小于 key 的路标；比全部路标都大时走最右的孩子。
    static std::size_t child_slot(const Node& node, int key) noexcept {
        const auto at = std::lower_bound(node.keys.begin(), node.keys.end(), key);
        const auto slot = static_cast<std::size_t>(at - node.keys.begin());
        return slot < node.keys.size() ? slot : node.keys.size() - 1;
    }

    bool insert_into(Node* node, int key, std::string value, Split& split) {
        mark_read(node);
        if (node->leaf) {
            const auto at = std::lower_bound(node->keys.begin(), node->keys.end(), key);
            const auto pos = static_cast<std::size_t>(at - node->keys.begin());
            mark_dirty(node);
            if (at != node->keys.end() && *at == key) {
                node->values[pos] = std::move(value);
                return false;
            }
            node->keys.insert(at, key);
            node->values.insert(node->values.begin() + static_cast<std::ptrdiff_t>(pos), std::move(value));
            if (entries(*node) > max_entries(*node)) {
                split_node(node, split);
            }
            return true;
        }

        const std::size_t slot = child_slot(*node, key);
        if (key > node->keys[slot]) {
            // 比这棵子树原来的最大码还大（只会发生在最右孩子）：路标跟着改成新的最大码。
            node->keys[slot] = key;
            mark_dirty(node);
        }
        Split child_split;
        const bool added =
            insert_into(node->children[slot].get(), key, std::move(value), child_split);
        if (child_split.happened) {
            adopt_split(node, slot, child_split);
            if (entries(*node) > max_entries(*node)) {
                split_node(node, split);
            }
        }
        return added;
    }

// >>> bplus-split
    /// 结点放进第 m+1 项：左半留 ⌈(m+1)/2⌉ 项，其余搬进新结点。叶和内部结点一视同仁——
    /// 本定义下内部结点的关键码与孩子一一对应，跟着孩子走就行，不存在「中位数上移」。
    void split_node(Node* node, Split& split) {
        const std::size_t keep = (entries(*node) + 1) / 2;
        const auto cut = static_cast<std::ptrdiff_t>(keep);
        auto right = std::make_unique<Node>(node->leaf);
        right->keys.assign(node->keys.begin() + cut, node->keys.end());
        node->keys.resize(keep);
        if (node->leaf) {
            right->values.assign(std::make_move_iterator(node->values.begin() + cut),
                                 std::make_move_iterator(node->values.end()));
            node->values.resize(keep);
            right->next = node->next;
            node->next = right.get();
        } else {
            right->children.assign(std::make_move_iterator(node->children.begin() + cut),
                                   std::make_move_iterator(node->children.end()));
            node->children.resize(keep);
        }
        mark_dirty(node);
        mark_dirty(right.get());
        split.happened = true;
        split.right = std::move(right);
    }

    /// 父结点收下分裂出的右半：原路标（右半的上界）留给右半，左半换成自己的新最大码。
    void adopt_split(Node* parent, std::size_t slot, Split& child_split) {
        const auto at = static_cast<std::ptrdiff_t>(slot);
        const int right_bound = parent->keys[slot];
        parent->keys[slot] = parent->children[slot]->keys.back();
        parent->keys.insert(parent->keys.begin() + at + 1, right_bound);
        parent->children.insert(parent->children.begin() + at + 1, std::move(child_split.right));
        mark_dirty(parent);
    }
// <<< bplus-split

    bool erase_from(Node* node, int key) {
        mark_read(node);
        if (node->leaf) {
            const auto at = std::lower_bound(node->keys.begin(), node->keys.end(), key);
            if (at == node->keys.end() || *at != key) {
                return false;
            }
            const auto pos = static_cast<std::size_t>(at - node->keys.begin());
            node->keys.erase(at);
            node->values.erase(node->values.begin() + static_cast<std::ptrdiff_t>(pos));
            mark_dirty(node);
            return true;
        }

        const std::size_t slot = child_slot(*node, key);
        if (key > node->keys[slot]) {
            return false;  // 比全树最大码还大，不可能在树里
        }
        if (!erase_from(node->children[slot].get(), key)) {
            return false;
        }
        // 没有下溢时上层路标不动：它只是上界，不必对应一个真实存在的关键码（原书 11.4.2）。
        if (entries(*node->children[slot]) < min_entries(*node->children[slot])) {
            rebalance(node, slot);
        }
        return true;
    }

    /// 下溢处理：先看左兄弟、再看右兄弟能否借一项；都借不到就合并（有左兄弟就与左兄弟合并）。
    void rebalance(Node* parent, std::size_t slot) {
        Node* child = parent->children[slot].get();
        if (slot > 0) {
            Node* left = parent->children[slot - 1].get();
            mark_read(left);
            if (entries(*left) > min_entries(*left)) {
                borrow_from_left(parent, slot, left, child);
                return;
            }
        }
        if (slot + 1 < parent->children.size()) {
            Node* right = parent->children[slot + 1].get();
            mark_read(right);
            if (entries(*right) > min_entries(*right)) {
                borrow_from_right(parent, slot, child, right);
                return;
            }
        }
        merge(parent, slot > 0 ? slot - 1 : slot);
    }

    /// 左兄弟的最后一项过继给 child；父结点里左兄弟的路标改成它的新最大码。
    void borrow_from_left(Node* parent, std::size_t slot, Node* left, Node* child) {
        child->keys.insert(child->keys.begin(), left->keys.back());
        left->keys.pop_back();
        if (child->leaf) {
            child->values.insert(child->values.begin(), std::move(left->values.back()));
            left->values.pop_back();
        } else {
            child->children.insert(child->children.begin(), std::move(left->children.back()));
            left->children.pop_back();
        }
        parent->keys[slot - 1] = left->keys.back();
        mark_dirty(child);
        mark_dirty(left);
        mark_dirty(parent);
    }

    /// 右兄弟的第一项过继给 child；父结点里 child 的路标改成过继来的那一项的关键码。
    void borrow_from_right(Node* parent, std::size_t slot, Node* child, Node* right) {
        child->keys.push_back(right->keys.front());
        right->keys.erase(right->keys.begin());
        if (child->leaf) {
            child->values.push_back(std::move(right->values.front()));
            right->values.erase(right->values.begin());
        } else {
            child->children.push_back(std::move(right->children.front()));
            right->children.erase(right->children.begin());
        }
        parent->keys[slot] = child->keys.back();
        mark_dirty(child);
        mark_dirty(right);
        mark_dirty(parent);
    }

    /// 把 children[slot+1] 并进 children[slot]：合并后的结点沿用右边那个的路标，
    /// 父结点删去左边那个的路标和右边那个孩子；右边那一页释放，不用写回。
    void merge(Node* parent, std::size_t slot) {
        Node* left = parent->children[slot].get();
        std::unique_ptr<Node> right = std::move(parent->children[slot + 1]);
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
        if (left->leaf) {
            left->values.insert(left->values.end(), std::make_move_iterator(right->values.begin()),
                                std::make_move_iterator(right->values.end()));
            left->next = right->next;  // 叶链要接上，否则范围扫描会断
        } else {
            for (auto& child : right->children) {
                left->children.push_back(std::move(child));
            }
        }
        const auto at = static_cast<std::ptrdiff_t>(slot);
        parent->keys.erase(parent->keys.begin() + at);
        parent->children.erase(parent->children.begin() + at + 1);
        forget(right.get());
        mark_dirty(left);
        mark_dirty(parent);
    }

    /// 把 total 项切成每页 per_page 项；最后一页不够下限时从前一页匀，匀不开就并进前一页。
    static std::vector<std::size_t> page_sizes(std::size_t total, std::size_t per_page,
                                               std::size_t min_fill) {
        std::vector<std::size_t> sizes;
        for (std::size_t start = 0; start < total; start += per_page) {
            sizes.push_back(std::min(per_page, total - start));
        }
        if (sizes.size() >= 2 && sizes.back() < min_fill) {
            std::size_t& prev = sizes[sizes.size() - 2];
            const std::size_t both = prev + sizes.back();
            if (both >= 2 * min_fill) {
                sizes.back() = min_fill;
                prev = both - min_fill;
            } else {
                prev = both;  // 两页合起来不超过一页的容量
                sizes.pop_back();
            }
        }
        return sizes;
    }

    // 一次插入或删除内的页访问：读过的页、改过的页各只算一次，释放的页不写回。
    void begin_op() {
        op_reads_.clear();
        op_dirty_.clear();
    }
    void end_op() {
        reads_ += op_reads_.size();
        writes_ += op_dirty_.size();
    }
    void mark_read(const Node* node) {
        if (std::find(op_reads_.begin(), op_reads_.end(), node) == op_reads_.end()) {
            op_reads_.push_back(node);
        }
    }
    void mark_dirty(const Node* node) {
        if (std::find(op_dirty_.begin(), op_dirty_.end(), node) == op_dirty_.end()) {
            op_dirty_.push_back(node);
        }
    }
    void forget(const Node* node) {
        op_dirty_.erase(std::remove(op_dirty_.begin(), op_dirty_.end(), node), op_dirty_.end());
    }

    bool check_node(const Node* node, std::size_t depth, std::size_t& leaf_depth, bool is_root,
                    std::optional<int> low, std::optional<int> high) const {
        const std::size_t n = entries(*node);
        if (n > max_entries(*node) || (!is_root && n < min_entries(*node))) {
            return false;
        }
        if (!std::is_sorted(node->keys.begin(), node->keys.end()) ||
            std::adjacent_find(node->keys.begin(), node->keys.end()) != node->keys.end()) {
            return false;
        }
        // 每个关键码都落在 (low, high] 里：low 是左边那棵子树的路标，high 是自己的路标。
        for (const int key : node->keys) {
            if ((low && key <= *low) || (high && key > *high)) {
                return false;
            }
        }
        if (node->leaf) {
            if (node->values.size() != n || (!is_root && n == 0)) {
                return false;
            }
            if (leaf_depth == 0) {
                leaf_depth = depth;
            }
            return leaf_depth == depth;  // 所有叶必须同层
        }
        if (node->children.size() != n || (is_root && n < 2)) {
            return false;
        }
        for (std::size_t i = 0; i < n; ++i) {
            const std::optional<int> child_low = i == 0 ? low : std::optional<int>(node->keys[i - 1]);
            if (!check_node(node->children[i].get(), depth + 1, leaf_depth, false, child_low,
                            node->keys[i])) {
                return false;
            }
        }
        return true;
    }

    std::size_t order_;
    std::size_t leaf_order_;
    std::unique_ptr<Node> root_;
    Node* first_leaf_ = nullptr;
    std::size_t size_ = 0;
    mutable std::size_t reads_ = 0;
    std::size_t writes_ = 0;
    std::vector<const Node*> op_reads_;
    std::vector<const Node*> op_dirty_;
};
// <<< bplus-tree

}  // namespace dsa::index
