// 原书【代码6.1】【代码6.2】【算法6.3】至【算法6.5】【代码6.6】至【代码6.8】【算法6.9】【算法6.10】。
#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa {

// >>> general-tree
template <typename T>
class GeneralTree {
public:
    struct Node {
        T value;
        Node* child{nullptr};
        Node* sibling{nullptr};
        Node* parent{nullptr};

        explicit Node(const T& value) : value(value) {}
    };

    GeneralTree() = default;

    GeneralTree(const GeneralTree& other) : root_(clone(other.root_, nullptr)) {}

    GeneralTree& operator=(const GeneralTree& other) {
        if (this != &other) {
            GeneralTree copy(other);
            swap(copy);
        }
        return *this;
    }

    GeneralTree(GeneralTree&& other) noexcept : root_(other.release()) {}

    GeneralTree& operator=(GeneralTree&& other) noexcept {
        if (this != &other) {
            clear();
            root_ = other.release();
        }
        return *this;
    }

    ~GeneralTree() { clear(); }

    void swap(GeneralTree& other) noexcept {
        using std::swap;
        swap(root_, other.root_);
    }

    [[nodiscard]] Node* root() noexcept { return root_; }
    [[nodiscard]] const Node* root() const noexcept { return root_; }

    void create_root(const T& value) {
        clear();
        root_ = new Node(value);
    }

    Node* insert_first(Node* parent, const T& value) {
        if (parent == nullptr) {
            throw std::invalid_argument("parent must not be null");
        }
        Node* node = new Node(value);
        node->sibling = parent->child;
        node->parent = parent;
        parent->child = node;
        return node;
    }

    Node* insert_next(Node* node, const T& value) {
        if (node == nullptr) {
            throw std::invalid_argument("node must not be null");
        }
        Node* next = new Node(value);
        next->sibling = node->sibling;
        next->parent = node->parent;
        node->sibling = next;
        return next;
    }

    [[nodiscard]] Node* parent_of(Node* node) const noexcept {
        return node == nullptr ? nullptr : node->parent;
    }

    void delete_subtree(Node* node) {
        if (node == nullptr) {
            return;
        }

        Node** link = node->parent == nullptr ? &root_ : &node->parent->child;
        while (*link != nullptr && *link != node) {
            link = &(*link)->sibling;
        }
        if (*link != node) {
            throw std::invalid_argument("node is not part of this tree");
        }

        *link = node->sibling;
        node->sibling = nullptr;
        destroy(node);
    }

    void clear() noexcept {
        destroy(root_);
        root_ = nullptr;
    }

    template <class Visitor>
    void preorder(Visitor&& visitor) const {
        pre(root_, visitor);
    }

    template <class Visitor>
    void postorder(Visitor&& visitor) const {
        post(root_, visitor);
    }

    // >>> dual-tag
    /// 【算法6.10】带双标记位的先根次序表示 → 「左子/右兄」链式树。
    ///
    /// 顺序表示里每个结点只带两个标志位：`has_child`（原书 ltag == 0）和
    /// `has_sibling`（原书 rtag == 0）。光靠先根次序 + 这两位就能把链恢复出来，
    /// 靠的是先根次序的一条性质：**任何结点的子树都紧跟在它后面**，
    /// 子树排完才轮到它的下一个兄弟。
    ///
    /// 于是「谁是某个结点的右兄弟」这件事要等它整棵子树扫完才知道——用栈记着：
    /// 扫到 `has_sibling` 的结点就压栈；扫到没有孩子的结点（子树到头了）就弹一个出来，
    /// 把刚建的结点接成它的右兄弟。
    struct DualTagNode {
        T value;
        bool has_child;    ///< 原书 ltag == 0
        bool has_sibling;  ///< 原书 rtag == 0
    };

    [[nodiscard]] static GeneralTree from_dual_tag(const DualTagNode* nodes, std::size_t count) {
        GeneralTree tree;
        if (count == 0) {
            return tree;
        }
        if (nodes == nullptr) {
            throw std::invalid_argument("from_dual_tag: 结点数组是空指针");
        }

        // 原书用 `stack<TreeNode<T>*> aStack`，这里用 vector 当栈（见 unit.json 豁免）。
        std::vector<Node*> waiting;  // 已扫到、还等着接右兄弟的结点
        Node* current = new Node(nodes[0].value);
        tree.root_ = current;

        for (std::size_t i = 0; i + 1 < count; ++i) {
            if (nodes[i].has_sibling) {
                waiting.push_back(current);
            }
            Node* fresh = new Node(nodes[i + 1].value);
            if (nodes[i].has_child) {
                current->child = fresh;
                fresh->parent = current;
            } else {
                // 子树到头了：刚建的结点属于栈顶那个结点的右兄弟。
                //
                // 原书这里直接 `aStack.top()`，**没有判空**。标志位不自洽的输入
                // （例如全是 has_child=false、has_sibling=false）会让它对空栈取顶，
                // 那是未定义行为（证据见 legacy.md 缺陷 4）。这里判空并抛异常。
                if (waiting.empty()) {
                    delete fresh;
                    throw std::invalid_argument("from_dual_tag: 标志位不自洽，右兄弟无处安放");
                }
                Node* owner = waiting.back();
                waiting.pop_back();
                owner->sibling = fresh;
                fresh->parent = owner->parent;  // 兄弟与它共享同一个父结点
            }
            current = fresh;
        }
        // 先根次序里最后一个结点必是叶子，且没有下一个兄弟。
        if (nodes[count - 1].has_child || !waiting.empty()) {
            throw std::invalid_argument("from_dual_tag: 标志位不自洽，序列没有正常收尾");
        }
        return tree;
    }
    // <<< dual-tag

    template <class Visitor>
    void breadth_first(Visitor&& visitor) const {
        std::vector<Node*> queue;
        for (Node* node = root_; node != nullptr; node = node->sibling) {
            queue.push_back(node);
        }
        for (std::size_t index = 0; index < queue.size(); ++index) {
            visitor(queue[index]->value);
            for (Node* child = queue[index]->child; child != nullptr;
                 child = child->sibling) {
                queue.push_back(child);
            }
        }
    }

private:
    // Recursive destruction and traversals preserve the textbook presentation.
    // They have a Stack Overflow Risk for a pathologically deep tree.
    static void destroy(Node* node) noexcept {
        if (node == nullptr) {
            return;
        }
        destroy(node->child);
        destroy(node->sibling);
        delete node;
    }

    static Node* clone(const Node* node, Node* parent) {
        if (node == nullptr) {
            return nullptr;
        }
        Node* copy = new Node(node->value);
        copy->parent = parent;
        try {
            copy->child = clone(node->child, copy);
            copy->sibling = clone(node->sibling, parent);
        } catch (...) {
            destroy(copy);
            throw;
        }
        return copy;
    }

    template <class Visitor>
    static void pre(Node* node, Visitor& visitor) {
        for (; node != nullptr; node = node->sibling) {
            visitor(node->value);
            pre(node->child, visitor);
        }
    }

    template <class Visitor>
    static void post(Node* node, Visitor& visitor) {
        for (; node != nullptr; node = node->sibling) {
            post(node->child, visitor);
            visitor(node->value);
        }
    }

    Node* release() noexcept {
        Node* result = root_;
        root_ = nullptr;
        return result;
    }

    Node* root_{nullptr};
};
// <<< general-tree

// >>> disjoint-set
/// 【代码6.8】树的父指针表示与 union/find。
///
/// 合并用原书的**重量权衡合并规则**(weighted union rule)：
/// 「令含元素少的子集的树根指向含元素多的子集的根」。原书结点里那个
/// `int nCount; //子树元素数目` 就是为它准备的，这里对应 `size_`。
///
/// **不要换成「按秩合并」**：按秩比的是树高，按重量比的是元素个数，两者
/// 在同一组等价对上会长出**不同形状**的树。原书与课程习题都按重量口径出题
/// （课程第 6 章习题 8 要求「使用重量权衡合并规则与路径压缩」并给出父指针数组），
/// 换成按秩会让读者对不上答案。
class DisjointSet {
public:
    explicit DisjointSet(std::size_t count) : parent_(count), size_(count, 1) {
        for (std::size_t index = 0; index < count; ++index) {
            parent_[index] = index;
        }
    }

    std::size_t find(std::size_t index) {
        if (index >= parent_.size()) {
            throw std::out_of_range("disjoint-set index");
        }
        if (parent_[index] != index) {
            parent_[index] = find(parent_[index]);  // 【算法6.9】路径压缩
        }
        return parent_[index];
    }

    bool unite(std::size_t left, std::size_t right) {
        left = find(left);
        right = find(right);
        if (left == right) {
            return false;  // 已经同类：幂等的可预期失败（D-001 §3c）
        }
        // 重量权衡：小树挂到大树下。并列时把**值大的根**挂到值小的根下——
        // 原书没有规定并列怎么办，这个口径取自课程第 6 章习题 8 的原话
        // 「当两棵树规模同样大时，使结点值较大的根结点作为值较小的根结点的子结点」，
        // 这样书里的实现能直接用来核对那道题的答案。
        if (size_[left] < size_[right] || (size_[left] == size_[right] && left > right)) {
            std::swap(left, right);
        }
        parent_[right] = left;
        size_[left] += size_[right];
        return true;
    }

    [[nodiscard]] bool same(std::size_t left, std::size_t right) {
        return find(left) == find(right);
    }

    /// 某个元素所在集合的大小。原书 `nCount` 的对外读法，也让「重量」这件事可测。
    [[nodiscard]] std::size_t set_size(std::size_t index) { return size_[find(index)]; }

    /// 当前的父指针数组——课程习题要求画出的正是它。
    [[nodiscard]] const std::vector<std::size_t>& parents() const noexcept { return parent_; }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> size_;  // 原书 nCount：子树元素数目
};
// <<< disjoint-set

}  // namespace dsa
