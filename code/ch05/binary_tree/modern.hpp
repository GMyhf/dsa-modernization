// 二叉树与二叉搜索树 —— 原书【代码5.1】【代码5.2】【算法5.3】至【算法5.10】的现代化实现。
//
// 结点的左右链接正是本节教学内容，故用裸指针 + 显式五法则，所有权由树独占。
// 递归 DFS 保留原书的教学结构；极深/病态树会耗尽调用栈，见 legacy.md 的风险说明。
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dsa {

/// 本章手写的链式栈：迭代周游与深拷贝都用它代替调用栈。
/// 放在命名空间层，BinaryTree 与 BinarySearchTree 共用同一把。
template <typename U>
class LinkedStack {
    struct Entry {
        U value;
        Entry* next;
    };

public:
    LinkedStack() = default;
    LinkedStack(const LinkedStack&) = delete;
    LinkedStack& operator=(const LinkedStack&) = delete;
    ~LinkedStack() { clear(); }

    [[nodiscard]] bool empty() const noexcept { return top_ == nullptr; }
    void push(const U& value) { top_ = new Entry{value, top_}; }
    void push(U&& value) { top_ = new Entry{std::move(value), top_}; }
    [[nodiscard]] std::optional<U> pop() {
        if (top_ == nullptr) return std::nullopt;
        Entry* entry = top_;
        top_ = entry->next;
        U value = std::move(entry->value);
        delete entry;
        return value;
    }

private:
    void clear() noexcept {
        while (top_ != nullptr) {
            Entry* entry = top_;
            top_ = entry->next;
            delete entry;
        }
    }
    Entry* top_{nullptr};
};

// >>> binary-tree
template <typename T>
class BinaryTree {
public:
    struct Node {
        T value;
        Node* left{nullptr};
        Node* right{nullptr};

        template <typename U>
        explicit Node(U&& item) : value(std::forward<U>(item)) {}
    };

    BinaryTree() noexcept = default;
    BinaryTree(const BinaryTree& other) : root_(clone(other.root_)) {}
    BinaryTree& operator=(const BinaryTree& other) {
        if (this != &other) {
            BinaryTree copy(other);
            swap(copy);
        }
        return *this;
    }
    BinaryTree(BinaryTree&& other) noexcept : root_(other.release_root()) {}
    BinaryTree& operator=(BinaryTree&& other) noexcept {
        if (this != &other) {
            make_empty();
            root_ = other.release_root();
        }
        return *this;
    }
    ~BinaryTree() { make_empty(); }

    void swap(BinaryTree& other) noexcept {
        using std::swap;
        swap(root_, other.root_);
    }

    [[nodiscard]] bool empty() const noexcept { return root_ == nullptr; }
    [[nodiscard]] const Node* root() const noexcept { return root_; }
    [[nodiscard]] Node* root() noexcept { return root_; }

    /// 构造一棵新树并接管两个子树，强异常保证：根结点创建失败时两棵子树不动。
    template <typename U>
    void create_tree(U&& value, BinaryTree&& left = {}, BinaryTree&& right = {}) {
        Node* fresh = new Node(std::forward<U>(value));
        fresh->left = left.release_root();
        fresh->right = right.release_root();
        make_empty();
        root_ = fresh;
    }

    /// 后序释放整个树。递归删除与递归周游同样受树高限制。
    void make_empty() noexcept {
        destroy(root_);
        root_ = nullptr;
    }

    // Stack Overflow Risk: 以下递归周游忠实保留原书算法5.3；病态深树可能耗尽调用栈。
    template <typename Visitor>
    void preorder(Visitor&& visit) const { preorder_impl(root_, visit); }
    template <typename Visitor>
    void inorder(Visitor&& visit) const { inorder_impl(root_, visit); }
    template <typename Visitor>
    void postorder(Visitor&& visit) const { postorder_impl(root_, visit); }

    // 算法5.4 至 5.6：作为补充保留原书的手写栈式非递归周游。
    template <typename Visitor>
    void preorder_iterative(Visitor&& visit) const {
        LinkedStack<const Node*> pending;
        if (root_ != nullptr) pending.push(root_);
        while (auto node = pending.pop()) {
            visit((*node)->value);
            if ((*node)->right != nullptr) pending.push((*node)->right);
            if ((*node)->left != nullptr) pending.push((*node)->left);
        }
    }
    template <typename Visitor>
    void inorder_iterative(Visitor&& visit) const {
        LinkedStack<const Node*> pending;
        const Node* current = root_;
        while (current != nullptr || !pending.empty()) {
            while (current != nullptr) {
                pending.push(current);
                current = current->left;
            }
            current = *pending.pop();
            visit(current->value);
            current = current->right;
        }
    }
    template <typename Visitor>
    void postorder_iterative(Visitor&& visit) const {
        struct Frame { const Node* node; bool expanded; };
        LinkedStack<Frame> pending;
        if (root_ != nullptr) pending.push(Frame{root_, false});
        while (auto frame = pending.pop()) {
            if (frame->expanded) {
                visit(frame->node->value);
            } else {
                pending.push(Frame{frame->node, true});
                if (frame->node->right != nullptr) pending.push(Frame{frame->node->right, false});
                if (frame->node->left != nullptr) pending.push(Frame{frame->node->left, false});
            }
        }
    }

    /// 算法5.7 的层次周游。内部手写链式 FIFO，不以 STL queue 替代本章内容。
    template <typename Visitor>
    void level_order(Visitor&& visit) const {
        struct Pending { const Node* node; Pending* next; };
        Pending* front = nullptr;
        Pending* back = nullptr;
        auto enqueue = [&](const Node* node) {
            Pending* item = new Pending{node, nullptr};
            if (back == nullptr) front = item; else back->next = item;
            back = item;
        };
        try {
            if (root_ != nullptr) enqueue(root_);
            while (front != nullptr) {
                Pending* item = front;
                front = front->next;
                if (front == nullptr) back = nullptr;
                const Node* node = item->node;
                delete item;
                visit(node->value);
                if (node->left != nullptr) enqueue(node->left);
                if (node->right != nullptr) enqueue(node->right);
            }
        } catch (...) {
            while (front != nullptr) { Pending* item = front; front = front->next; delete item; }
            throw;
        }
    }

    [[nodiscard]] const Node* parent_of(const Node* wanted) const noexcept {
        return parent_of_impl(root_, wanted);
    }

    // >>> rebuild
    /// 由中序 + 后序序列重建二叉树（两个序列各 count 个元素，**关键码互不相同**）。
    ///
    /// 不自洽的输入一律抛 std::invalid_argument，绝不默默建出一棵错树：
    /// 中序里有重复键、后序里出现中序没有的键、某个根落在当前中序区间之外。
    /// 能建完就说明两个序列恰好是这棵树的中序与后序（归纳即得），不必再周游核对。
    /// 要求 T 支持 operator<（用来排序查位置）。时间 O(n log n)，额外空间 O(n)。
    static BinaryTree from_inorder_postorder(const T* inorder, const T* postorder, std::size_t count) {
        return rebuild(inorder, postorder, count, Order::postorder);
    }
    /// 由前序 + 中序序列重建（本章上机题第 1 题）。契约同上。
    static BinaryTree from_preorder_inorder(const T* preorder, const T* inorder, std::size_t count) {
        return rebuild(inorder, preorder, count, Order::preorder);
    }
    // <<< rebuild

private:
    enum class Order { preorder, postorder };

    // >>> rebuild-impl
    /// 两种重建共用的主体。**不递归**：待建的子树用手写链式栈保存为「区间帧」，
    /// 退化成链的输入也不会压穿调用栈。每一帧只记下标，不复制子数组。
    static BinaryTree rebuild(const T* inorder, const T* other, std::size_t count, Order order) {
        BinaryTree result;
        if (count == 0) return result;
        if (inorder == nullptr || other == nullptr) {
            throw std::invalid_argument("rebuild: non-empty sequence given as null pointer");
        }
        // 位置查找表：中序下标按键排序，之后二分查位置。排好序顺便查出重复键。
        std::unique_ptr<std::size_t[]> by_key(new std::size_t[count]);
        for (std::size_t i = 0; i < count; ++i) by_key[i] = i;
        const auto key_less = [inorder](std::size_t a, std::size_t b) { return inorder[a] < inorder[b]; };
        std::sort(by_key.get(), by_key.get() + count, key_less);
        for (std::size_t i = 1; i < count; ++i) {
            if (!(inorder[by_key[i - 1]] < inorder[by_key[i]])) {
                throw std::invalid_argument("rebuild: duplicate key in inorder sequence");
            }
        }
        const auto position_in_inorder = [&](const T& key) {
            const std::size_t* hit = std::lower_bound(by_key.get(), by_key.get() + count, key,
                [inorder](std::size_t index, const T& k) { return inorder[index] < k; });
            if (hit == by_key.get() + count || key < inorder[*hit]) {
                throw std::invalid_argument("rebuild: key missing from inorder sequence");
            }
            return *hit;
        };

        // 一帧 = 「把中序 [in_begin, in_end) 与另一序列 [other_begin, …) 建成子树，挂到 *link」。
        struct Frame { Node** link; std::size_t in_begin, in_end, other_begin; };
        LinkedStack<Frame> pending;
        pending.push(Frame{&result.root_, 0, count, 0});
        while (auto frame = pending.pop()) {
            const std::size_t size = frame->in_end - frame->in_begin;
            if (size == 0) continue;                         // 空子树：*link 已是 nullptr
            // 前序的根在区间最前，后序的根在区间最后。
            const std::size_t root_at = order == Order::preorder ? frame->other_begin
                                                                 : frame->other_begin + size - 1;
            const std::size_t k = position_in_inorder(other[root_at]);
            if (k < frame->in_begin || k >= frame->in_end) {
                throw std::invalid_argument("rebuild: root lies outside its inorder range");
            }
            Node* const node = new Node(other[root_at]);
            *frame->link = node;                             // 先挂上：再抛异常时由 result 统一释放
            const std::size_t left_size = k - frame->in_begin;
            // 左子树在另一序列里紧跟根（前序）或从区间头开始（后序），右子树接在左子树后面。
            const std::size_t left_other = order == Order::preorder ? frame->other_begin + 1
                                                                    : frame->other_begin;
            pending.push(Frame{&node->right, k + 1, frame->in_end, left_other + left_size});
            pending.push(Frame{&node->left, frame->in_begin, k, left_other});
        }
        return result;
    }
    // <<< rebuild-impl

    // >>> iterative-destroy
    /// 释放整棵树。**迭代实现**，栈深度恒定。
    ///
    /// 递归版 `destroy(left); destroy(right); delete node;` 在退化成链的树上会压穿栈——
    /// 实测纯左链 100 万结点即段错误（`collab/UNVERIFIED-RISKS.md` 有复现方法）。
    /// 这里用「右旋到没有左孩子，再沿右链删」的经典办法：每次旋转把左子树提上来，
    /// 树被逐步拉直成一条右链，然后一个一个删。总代价仍是 O(n)，额外空间 O(1)，
    /// 而且不分配内存，所以能保持 noexcept。
    static void destroy(Node* node) noexcept {
        while (node != nullptr) {
            if (node->left != nullptr) {
                Node* const left = node->left;   // 右旋：左孩子成为新的根
                node->left = left->right;
                left->right = node;
                node = left;
            } else {
                Node* const right = node->right;
                delete node;
                node = right;
            }
        }
    }
    // <<< iterative-destroy
    /// 深拷贝整棵树。**迭代实现**，用显式栈代替调用栈。
    ///
    /// 递归版在退化树上同样会压穿栈，而且比 destroy 更早——实测纯左链 50 万结点即段错误。
    /// 显式栈的结点放在堆上，深度不再受线程栈限制；中途抛异常时回收已建好的部分，保持强异常保证。
    static Node* clone(const Node* node) {
        if (node == nullptr) {
            return nullptr;
        }
        Node* copy_root = new Node(node->value);
        try {
            // 用本章自己那把手写链式栈，与迭代周游同一套零件。
            LinkedStack<std::pair<const Node*, Node*>> pending;
            pending.push({node, copy_root});
            while (auto item = pending.pop()) {
                const Node* const source = item->first;
                Node* const target = item->second;
                if (source->left != nullptr) {
                    target->left = new Node(source->left->value);
                    pending.push({source->left, target->left});
                }
                if (source->right != nullptr) {
                    target->right = new Node(source->right->value);
                    pending.push({source->right, target->right});
                }
            }
        } catch (...) {
            destroy(copy_root);
            throw;
        }
        return copy_root;
    }
    static const Node* parent_of_impl(const Node* node, const Node* wanted) noexcept {
        if (node == nullptr || wanted == nullptr) return nullptr;
        if (node->left == wanted || node->right == wanted) return node;
        if (const Node* left = parent_of_impl(node->left, wanted)) return left;
        return parent_of_impl(node->right, wanted);
    }
    template <typename Visitor> static void preorder_impl(const Node* node, Visitor& visit) {
        if (node != nullptr) { visit(node->value); preorder_impl(node->left, visit); preorder_impl(node->right, visit); }
    }
    template <typename Visitor> static void inorder_impl(const Node* node, Visitor& visit) {
        if (node != nullptr) { inorder_impl(node->left, visit); visit(node->value); inorder_impl(node->right, visit); }
    }
    template <typename Visitor> static void postorder_impl(const Node* node, Visitor& visit) {
        if (node != nullptr) { postorder_impl(node->left, visit); postorder_impl(node->right, visit); visit(node->value); }
    }
    Node* release_root() noexcept { Node* result = root_; root_ = nullptr; return result; }
    Node* root_{nullptr};
};
// <<< binary-tree

// >>> bst
template <typename T, typename Compare = std::less<T>>
class BinarySearchTree {
    struct Node {
        T value;
        Node* left{nullptr};
        Node* right{nullptr};
        template <typename U> explicit Node(U&& item) : value(std::forward<U>(item)) {}
    };

public:
    BinarySearchTree() = default;
    BinarySearchTree(const BinarySearchTree& other) : root_(clone(other.root_)), compare_(other.compare_) {}
    BinarySearchTree& operator=(const BinarySearchTree& other) {
        if (this != &other) { BinarySearchTree copy(other); swap(copy); }
        return *this;
    }
    BinarySearchTree(BinarySearchTree&& other) : root_(other.root_), compare_(std::move(other.compare_)) { other.root_ = nullptr; }
    BinarySearchTree& operator=(BinarySearchTree&& other) {
        if (this != &other) { clear(); root_ = other.root_; other.root_ = nullptr; compare_ = std::move(other.compare_); }
        return *this;
    }
    ~BinarySearchTree() { clear(); }

    void swap(BinarySearchTree& other) {
        using std::swap; swap(root_, other.root_); swap(compare_, other.compare_);
    }
    [[nodiscard]] bool empty() const noexcept { return root_ == nullptr; }

    /// 算法5.9：插入唯一键。重复键是可预期状态，返回 false。
    bool insert(const T& value) { return insert_impl(value); }
    bool insert(T&& value) { return insert_impl(std::move(value)); }

    [[nodiscard]] bool contains(const T& value) const {
        const Node* current = root_;
        while (current != nullptr) {
            if (equivalent(value, current->value)) return true;
            current = compare_(value, current->value) ? current->left : current->right;
        }
        return false;
    }

    /// 算法5.10：删除不存在的键是幂等的可预期状态，返回 false（D-001 §3c）。
    bool remove(const T& value) { return remove_impl(root_, value); }
    void clear() noexcept { destroy(root_); root_ = nullptr; }

    template <typename Visitor>
    void inorder(Visitor&& visit) const { inorder_impl(root_, visit); }

private:
    [[nodiscard]] bool equivalent(const T& left, const T& right) const {
        return !compare_(left, right) && !compare_(right, left);
    }
    template <typename U> bool insert_impl(U&& value) {
        Node** link = &root_;
        while (*link != nullptr) {
            if (equivalent(value, (*link)->value)) return false;
            link = compare_(value, (*link)->value) ? &(*link)->left : &(*link)->right;
        }
        *link = new Node(std::forward<U>(value));
        return true;
    }
    bool remove_impl(Node*& link, const T& value) {
        if (link == nullptr) return false;
        if (compare_(value, link->value)) return remove_impl(link->left, value);
        if (compare_(link->value, value)) return remove_impl(link->right, value);
        Node* removed = link;
        if (removed->left == nullptr) { link = removed->right; delete removed; return true; }
        Node** predecessor_link = &removed->left;
        while ((*predecessor_link)->right != nullptr) predecessor_link = &(*predecessor_link)->right;
        Node* replacement = *predecessor_link;
        *predecessor_link = replacement->left;
        replacement->left = removed->left;
        replacement->right = removed->right;
        link = replacement;
        delete removed;
        return true;
    }
    /// 释放整棵树（与 BinaryTree 同法）。**迭代实现**，栈深度恒定。
    ///
    /// 递归版 `destroy(left); destroy(right); delete node;` 在退化成链的树上会压穿栈——
    /// 实测纯左链 100 万结点即段错误（`collab/UNVERIFIED-RISKS.md` 有复现方法）。
    /// 这里用「右旋到没有左孩子，再沿右链删」的经典办法：每次旋转把左子树提上来，
    /// 树被逐步拉直成一条右链，然后一个一个删。总代价仍是 O(n)，额外空间 O(1)，
    /// 而且不分配内存，所以能保持 noexcept。
    static void destroy(Node* node) noexcept {
        while (node != nullptr) {
            if (node->left != nullptr) {
                Node* const left = node->left;   // 右旋：左孩子成为新的根
                node->left = left->right;
                left->right = node;
                node = left;
            } else {
                Node* const right = node->right;
                delete node;
                node = right;
            }
        }
    }
    /// 深拷贝（与 BinaryTree 同法：显式栈代替调用栈，退化树上不压穿栈）。
    static Node* clone(const Node* node) {
        if (node == nullptr) {
            return nullptr;
        }
        Node* copy_root = new Node(node->value);
        try {
            // 用本章自己那把手写链式栈，与迭代周游同一套零件。
            LinkedStack<std::pair<const Node*, Node*>> pending;
            pending.push({node, copy_root});
            while (auto item = pending.pop()) {
                const Node* const source = item->first;
                Node* const target = item->second;
                if (source->left != nullptr) {
                    target->left = new Node(source->left->value);
                    pending.push({source->left, target->left});
                }
                if (source->right != nullptr) {
                    target->right = new Node(source->right->value);
                    pending.push({source->right, target->right});
                }
            }
        } catch (...) {
            destroy(copy_root);
            throw;
        }
        return copy_root;
    }
    template <typename Visitor> static void inorder_impl(const Node* node, Visitor& visit) {
        if (node != nullptr) { inorder_impl(node->left, visit); visit(node->value); inorder_impl(node->right, visit); }
    }
    Node* root_{nullptr};
    Compare compare_{};
};
// <<< bst

}  // namespace dsa
