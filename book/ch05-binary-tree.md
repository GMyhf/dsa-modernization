# 第5章 二叉树

本章保留二叉链表、递归周游、二叉搜索树、完全二叉树上的堆与 Huffman 合并的教学骨架。
现代化处理的是所有权、空状态和错误接口：树独占结点并显式深拷贝；提取返回 `optional`；
按键删除返回 `bool`；容器不输出文本。

## 5.2 二叉树的周游与 5.3 链式存储

【代码5.1】二叉树结点的抽象数据类型。

【代码5.2】二叉树的抽象数据类型。

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

【代码5.1结束】

【代码5.2结束】

【算法5.3】深度优先周游二叉树或其子树。

前/中/后序递归实现保持原书的 tLR、LtR、LRt 结构。D-001 §3d 规定它是主教学实现；
**Stack Overflow Risk**：病态深树的递归深度受进程调用栈限制，生产场景应按限制选择显式栈补充实现。

【算法5.3结束】

【算法5.4】非递归前序周游二叉树或其子树。

【算法5.4结束】

【算法5.5】非递归中序周游二叉树或其子树。

【算法5.5结束】

【算法5.6】非递归后序周游二叉树或其子树。

【算法5.6结束】

【算法5.7】广度周游二叉树及其子树。

层次周游使用源码中的手写链式 FIFO，访问顺序为逐层、从左到右。

【算法5.7结束】

【代码5.8】二叉树部分成员函数的实现。

本清单的 OCR 缺失结束标记。现代书稿按 `dsa_raw.md:4105` 的“删除根结点”注释收尾，
因为下一行 `4106` 已开始 5.3.2 的顺序存储主题；完整依据见 `legacy.md`。

【代码5.8结束】

### 递归的代价：一个可以量出来的数字

递归周游是本节的教学主线，本书保留了它。但"递归受调用栈限制"这句话，
作为教材说到这里是不够的——**它到底能撑多深，是可以量出来的**。

在一台 Linux 机器上（gcc 13.3，`ulimit -s` 为 8 MB 的默认栈），
用一条纯左链（深度等于结点数）实测：

| 构建 | 递归析构 | 递归前序周游 |
| --- | --- | --- |
| Release `-O2` | 50 万深度通过，100 万崩 | 50 万通过，100 万崩 |
| Debug + ASan/UBSan | 50 万通过，100 万崩 | **50 万就崩** |

Debug 档更早崩，因为检测工具让每层栈帧变胖。换一台机器、换一个栈大小，
这些数字都会变——**重点不是这几个数，而是"它是有限的、而且可以测"**。

更值得记住的是**崩的时候你看到什么**：

- 开了 sanitizer 的构建会明确告诉你
  `AddressSanitizer: stack-overflow`，并给出一路递归下去的回溯，直指出事的那一行；
- 而 Release 构建只有一个段错误，**一行解释都没有**。

还有一点容易被忽略：`preorder` 这类周游是你**显式调用**的，而**析构和拷贝
也在递归**——一个普通的作用域结束、一次拷贝赋值，都会悄悄走同样深的递归，
调用方看不到任何迹象。本书为三种周游提供了显式栈的迭代版本作为补充，
但**析构与拷贝没有**：真正先撞墙的恰恰是它们。

（本书未验证的其余风险点，连同复现方法，集中记在仓库的
`collab/UNVERIFIED-RISKS.md`。）

## 5.4 二叉搜索树

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

【算法5.9】二叉搜索树的结点插入算法。

【算法5.9结束】

【算法5.10】改进的二叉搜索树的结点删除。

删除有左子树的结点时摘取左子树最大结点作为前驱替代者；先从旧位置脱离它，再接管被删结点
的左右子树，最后释放被删结点。键不存在返回 `false`，不抛异常。

【算法5.10结束】

## 5.5 堆与 5.6 Huffman 树

【代码5.11】堆的类定义和筛选法。

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
        try { for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]); }
        catch (...) { delete[] fresh; throw; }
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

【代码5.11结束】

【代码5.12】Huffman 树的类定义。

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

【代码5.12结束】
