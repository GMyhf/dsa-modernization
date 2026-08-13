# 第5章 二叉树

二叉树的每个结点最多有左、右两个孩子。周游回答「以什么顺序访问全部结点」；二叉搜索树加上左小右大；堆把最小（或最大）值放在根；Huffman 树不断合并两个最小权值。

源码：[二叉树与二叉搜索树](../code/ch05/binary_tree/modern.hpp)、
[最小堆与 Huffman 树](../code/ch05/heap_huffman/modern.hpp)、
[树的示例](../code/ch05/binary_tree/demo.cpp)、
[堆与 Huffman 的示例](../code/ch05/heap_huffman/demo.cpp)。

## 5.1 二叉树的概念

二叉树由结点的有限集合构成：或者为空，或者由一个根和两棵互不相交的左、右子树组成。左右次序不能颠倒。根没有父结点；其余每个结点恰有一个父结点，至多两个孩子。没有孩子的是叶，其余是内部结点。从根到某结点的边数是该结点的层数，根在第 0 层。

### 5.1.1 定义和基本术语

下面这棵树：

```text
      A
     / \
    B   C
   / \
  D   E
```

![图 5.5 二叉树示例](assets/7c6579b015042738.jpg)

图 5.5 二叉树示例

四种周游访问的是同一组结点，次序不同：

| 周游 | 顺序 | 本例 |
| --- | --- | --- |
| 先序 | 根，左，右 | A B D E C |
| 中序 | 左，根，右 | D B E A C |
| 后序 | 左，右，根 | D E B C A |
| 层次 | 按离根的距离 | A B C D E |

递归版最贴合定义，也是 5.2 节要教的东西。极深的退化树会耗尽调用栈：Release 档大约在百万层段错误且**没有诊断**，ASan 档会打印 `stack-overflow` 并指到具体行。析构和拷贝走的也是递归，调用方看不见。数字和复现程序见仓库中的未验证风险说明。

### 5.1.2 满二叉树、完全二叉树、扩充二叉树

任何结点或者是叶，或者左右子树都非空，叫做满二叉树。叶只出现在最下两层、且最下层靠左对齐，叫做完全二叉树。在空子树位置补上空树叶，得到扩充二叉树；外部路径长度 $E$ 与内部路径长度 $I$ 满足 $E = I + 2n$。

### 5.1.3 主要性质

第 $i$ 层至多 $2^i$ 个结点；深度为 $k$ 的二叉树至多 $2^{k+1}-1$ 个结点；叶结点数 $n_0 = n_2 + 1$。$n$ 个结点的完全二叉树高度为 $\lceil\log_2(n+1)\rceil$。按层从 0 编号时，结点 $i$ 的父是 $\lfloor(i-1)/2\rfloor$，左右孩子是 $2i+1$ 与 $2i+2$。这些性质没错，原样保留。

## 5.2 二叉树的周游

### 5.2.1 先跑一遍

先建树并打印四种周游，再插一棵 BST：

```cpp file=code/ch05/binary_tree/demo.cpp
#include "modern.hpp"

#include <iostream>
#include <utility>

int main() {
    dsa::BinaryTree<char> left_leaf;
    dsa::BinaryTree<char> right_leaf;
    dsa::BinaryTree<char> left;
    dsa::BinaryTree<char> right;
    dsa::BinaryTree<char> root;
    left_leaf.create_tree('D');
    right_leaf.create_tree('E');
    left.create_tree('B', std::move(left_leaf), std::move(right_leaf));
    right.create_tree('C');
    root.create_tree('A', std::move(left), std::move(right));

    std::cout << "先序: ";
    root.preorder([](char value) { std::cout << value; });
    std::cout << "\n中序: ";
    root.inorder([](char value) { std::cout << value; });
    std::cout << "\n后序: ";
    root.postorder([](char value) { std::cout << value; });
    std::cout << "\n层次: ";
    root.level_order([](char value) { std::cout << value; });
    std::cout << '\n';

    dsa::BinarySearchTree<int> tree;
    for (int key : {8, 3, 10, 1, 6, 14, 4, 7}) {
        (void)tree.insert(key);
    }
    std::cout << "BST 中序:";
    tree.inorder([](int key) { std::cout << ' ' << key; });
    std::cout << "\n含 6? " << (tree.contains(6) ? "是" : "否")
              << "  删 3 后含 3? ";
    (void)tree.remove(3);
    std::cout << (tree.contains(3) ? "是" : "否") << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch05/binary_tree \
    code/ch05/binary_tree/demo.cpp -o /tmp/tree-demo
/tmp/tree-demo
```

```console
先序: ABDEC
中序: DBEAC
后序: DEBCA
层次: ABCDE
BST 中序: 1 3 4 6 7 8 10 14
含 6? 是  删 3 后含 3? 否
```

`create_tree(value, left, right)` 接管两棵子树。参数是右值，表示所有权被移走；`left_leaf` 在 `std::move` 之后变空，不会和 `left` 抢着析构同一结点。

### 5.2.2 深度优先周游

先序 / 中序 / 后序的递归版就是三行：访问自己与走进左右孩子的次序不同。迭代版用一棵手写链式栈模拟调用栈，按 D-001 §3d 只作补充，不替换递归主实现。

### 5.2.3 广度优先周游

层次周游用手写链式 FIFO，不用 `std::queue` 替代本节要教的队列用法。

## 5.3 二叉树的存储结构

`create_tree` 先 `new` 出新根，再把两棵子树的根指针挪过来，最后才清空自己原来的树。这样即使 `new` 抛异常，调用方的两棵子树也不动。链式存储是本节主实现；完全二叉树还可以按 5.1.3 的编号放进数组，堆就是这种用法。

```cpp file=code/ch05/binary_tree/modern.hpp#binary-tree
template <typename T>
class BinaryTree {
private:
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

private:
    static void destroy(Node* node) noexcept {
        if (node != nullptr) { destroy(node->left); destroy(node->right); delete node; }
    }
    static Node* clone(const Node* node) {
        if (node == nullptr) return nullptr;
        Node* copy = new Node(node->value);
        try { copy->left = clone(node->left); copy->right = clone(node->right); }
        catch (...) { destroy(copy); throw; }
        return copy;
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
```

## 5.4 二叉搜索树

二叉搜索树要求左子树的键都小于根、右子树都大于根。中序周游因此正好是排序。插入重复键、删除不存在的键都是可预期状态，返回 `false`，不抛异常。删除有左右孩子的结点时，用左子树里最右的前驱替换它：先把前驱从原位置摘下，再让它继承被删结点的两棵子树，最后只 `delete` 被删结点一次。漏掉「先脱离原父」会形成环或二次释放。

```cpp file=code/ch05/binary_tree/modern.hpp#bst
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
    static void destroy(Node* node) noexcept { if (node != nullptr) { destroy(node->left); destroy(node->right); delete node; } }
    static Node* clone(const Node* node) {
        if (node == nullptr) return nullptr;
        Node* copy = new Node(node->value);
        try { copy->left = clone(node->left); copy->right = clone(node->right); }
        catch (...) { destroy(copy); throw; }
        return copy;
    }
    template <typename Visitor> static void inorder_impl(const Node* node, Visitor& visit) {
        if (node != nullptr) { inorder_impl(node->left, visit); visit(node->value); inorder_impl(node->right, visit); }
    }
    Node* root_{nullptr};
    Compare compare_{};
};
```

## 5.5 堆与优先队列

最小堆是一棵完全二叉树，父结点不大于孩子；用数组存时，下标 `i` 的孩子是 `2i+1` 和 `2i+2`。`sift_down` 必须比较左右两个孩子。空堆上 `remove_min()` 返回 `nullopt`。

```cpp file=code/ch05/heap_huffman/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::MinHeap<int> heap;
    for (int value : {5, 1, 4, 2}) {
        heap.insert(value);
    }
    std::cout << "依次取出最小元:";
    while (auto value = heap.remove_min()) {
        std::cout << ' ' << *value;
    }
    std::cout << '\n';

    const int weights[] = {2, 3, 4, 7};
    const dsa::HuffmanTree tree(weights, 4);
    std::cout << "权 2,3,4,7 的 Huffman 树根权 = " << tree.total_weight() << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch05/heap_huffman \
    code/ch05/heap_huffman/demo.cpp -o /tmp/heap-demo
/tmp/heap-demo
```

```console
依次取出最小元: 1 2 4 5
权 2,3,4,7 的 Huffman 树根权 = 16
```

```cpp file=code/ch05/heap_huffman/modern.hpp#min-heap
template <typename T>
class MinHeap {
public:
    static_assert(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value,
                  "MinHeap growth relies on non-throwing moves; use a noexcept-movable element type.");

    MinHeap() = default;
    MinHeap(const MinHeap& other) : data_(other.capacity_ ? new T[other.capacity_] : nullptr), size_(other.size_), capacity_(other.capacity_) {
        try { for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i]; }
        catch (...) { delete[] data_; throw; }
    }
    MinHeap& operator=(const MinHeap& other) { if (this != &other) { MinHeap copy(other); swap(copy); } return *this; }
    MinHeap(MinHeap&& other) noexcept { swap(other); }
    MinHeap& operator=(MinHeap&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = other.capacity_ = 0;
        }
        return *this;
    }
    ~MinHeap() { delete[] data_; }
    void swap(MinHeap& other) noexcept { using std::swap; swap(data_, other.data_); swap(size_, other.size_); swap(capacity_, other.capacity_); }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void insert(const T& value) { ensure_capacity(); data_[size_] = value; sift_up(size_++); }
    void insert(T&& value) { ensure_capacity(); data_[size_] = std::move(value); sift_up(size_++); }
    [[nodiscard]] std::optional<T> remove_min() {
        if (empty()) return std::nullopt;
        T value = std::move(data_[0]);
        --size_;
        if (size_ == 0) return value;
        data_[0] = std::move(data_[size_]);
        sift_down(0);
        return value;
    }
private:
    void ensure_capacity() {
        if (size_ < capacity_) return;
        const std::size_t next = capacity_ == 0 ? 4 : capacity_ * 2;
        T* fresh = new T[next];
        // The class contract requires non-throwing move assignment, so the
        // migration loop cannot fail. Allocation failure is thrown before fresh exists.
        for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
    }
    void sift_up(std::size_t index) {
        while (index != 0 && data_[index] < data_[(index - 1) / 2]) {
            using std::swap;
            swap(data_[index], data_[(index - 1) / 2]);
            index = (index - 1) / 2;
        }
    }
    void sift_down(std::size_t index) {
        for (;;) {
            const std::size_t left = index * 2 + 1;
            const std::size_t right = left + 1;
            std::size_t smallest = index;
            if (left < size_ && data_[left] < data_[smallest]) smallest = left;
            if (right < size_ && data_[right] < data_[smallest]) smallest = right;
            if (smallest == index) return;
            using std::swap;
            swap(data_[index], data_[smallest]);
            index = smallest;
        }
    }
    T* data_{nullptr};
    std::size_t size_{0};
    std::size_t capacity_{0};
};
```

## 5.6 Huffman 树及其应用

Huffman 树反复取出两个最小权，合成它们的和，直到只剩一棵——这就是前缀编码的那棵树。根权等于全部叶子权之和。合并时若 `new` 父结点失败，会拆开并销毁已经取出的两棵子树。

```cpp file=code/ch05/heap_huffman/modern.hpp#huffman
class HuffmanTree {
    struct Node { int weight; Node* left{nullptr}; Node* right{nullptr}; explicit Node(int w):weight(w){} };
    struct ByWeight { Node* node{nullptr}; bool operator<(const ByWeight& other) const noexcept { return node->weight < other.node->weight; } };
public:
    HuffmanTree()=default;
    explicit HuffmanTree(const int* weights, std::size_t count) {
        if (count == 0) return;
        if (weights == nullptr) throw std::invalid_argument("non-empty Huffman input requires weights");
        MinHeap<ByWeight> heap;
        try {
            for (std::size_t i = 0; i < count; ++i) {
                if (weights[i] < 0) throw std::invalid_argument("Huffman weights must be non-negative");
                Node* leaf = new Node(weights[i]);
                try { heap.insert(ByWeight{leaf}); }
                catch (...) { delete leaf; throw; }
            }
            while (heap.size() > 1) {
                Node* left = heap.remove_min()->node;
                Node* right = heap.remove_min()->node;
                Node* parent = nullptr;
                try {
                    if (left->weight > std::numeric_limits<int>::max() - right->weight) {
                        throw std::overflow_error("Huffman weight sum overflows int");
                    }
                    parent = new Node(left->weight + right->weight);
                    parent->left = left;
                    parent->right = right;
                    heap.insert(ByWeight{parent});
                } catch (...) {
                    if (parent != nullptr) { parent->left = parent->right = nullptr; delete parent; }
                    destroy(left);
                    destroy(right);
                    throw;
                }
            }
            root_ = heap.remove_min()->node;
        } catch (...) {
            while (auto item = heap.remove_min()) destroy(item->node);
            throw;
        }
    }
    HuffmanTree(const HuffmanTree&)=delete; HuffmanTree& operator=(const HuffmanTree&)=delete;
    HuffmanTree(HuffmanTree&& other)noexcept:root_(other.root_){other.root_=nullptr;} HuffmanTree& operator=(HuffmanTree&& other)noexcept{if(this!=&other){destroy(root_);root_=other.root_;other.root_=nullptr;}return *this;} ~HuffmanTree(){destroy(root_);} [[nodiscard]] int total_weight()const noexcept{return root_?root_->weight:0;}
private: static void destroy(Node*n)noexcept{if(n){destroy(n->left);destroy(n->right);delete n;}} Node* root_{nullptr};
};
```

