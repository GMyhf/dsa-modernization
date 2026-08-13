# 第6章 树

一般树允许一个结点有任意多个孩子。本章用「左孩子、右兄弟」表示它：`child` 指向第一个孩子，`sibling` 指向下一个兄弟。并查集则回答另一类问题：若干元素分属哪些互不相交的集合。

源码：[一般树和并查集](../code/ch06/general_tree/modern.hpp)、
[可运行示例](../code/ch06/general_tree/demo.cpp)、
[测试](../code/ch06/general_tree/test.cpp)。

## 6.1 树的定义和基本术语

树允许一个结点有任意多个孩子。森林是若干互不相交的树。任意树可以与二叉树互相转换：把兄弟连起来、只保留长子指针，再顺时针旋转，就得到左孩子/右兄弟那棵二叉树。

### 6.1.1 树和森林

例如 A 的孩子是 B、C、D，B 的孩子是 E、F。画成二叉树形状之后：

```text
A
└─ child → B ── sibling → C ── sibling → D
            └─ child → E ── sibling → F
```

任意度树只需两个指针域。代价是不能 O(1) 取得「第 k 个孩子」，必须沿兄弟链走过去。

图6.7 就是上面这张「左子/右兄」图。

三种周游的访问顺序不同：

| 周游 | 顺序 | 本例 |
| --- | --- | --- |
| 先根 | 结点，再孩子子树 | A B E F C D |
| 后根 | 孩子子树，再结点 | E F B C D A |
| 层次 | 按离根的距离 | A B C D E F |

并查集只回答两类问题：元素属于哪个集合，以及两个元素是否同集合。`find(x)` 沿父指针走到根；路径压缩让沿途结点以后直接指向根。`unite(a, b)` 把两个根合并，并优先把较矮的树挂在较高的树下。

递归周游与销毁在极深树上有栈溢出风险。

度为 2 的有序树还不是二叉树：删掉第一个孩子后，第二个会顶上来。二叉树必须严格区分左右空位。

### 6.1.2 森林与二叉树的等价转换

把每棵树的兄弟从左到右用右指针串起来，只保留长子作为左指针，再顺时针旋转，森林就变成一棵二叉树。反过来：二叉树的左孩子是长子，右孩子是下一个兄弟，拆开就还原成森林。这两种转换互逆，原书没错。

### 6.1.3 树的抽象数据类型

树的运算是：建根、插入第一个孩子、插入下一个兄弟、问父结点、删子树、三种周游。本书把它们直接写在 `GeneralTree` 上。

### 6.1.4 树的周游

先根先访问结点再进孩子子树；后根相反；层次按离根距离。这三种都没错，实现见 6.2。

## 6.2 树的链式存储结构

### 6.2.1 「子结点表」表示方法

每个结点保存一块孩子指针数组。取第 k 个孩子是 O(1)，但度不固定时要扩容，插入兄弟也要搬数组。本章不把它当主实现。

### 6.2.2 静态「左子/右兄」表示法

用数组下标代替指针，适合结点数事先已知的场合。语义与动态版相同。

### 6.2.3 动态表示法

每个结点单独 `new`，用指针相连。长度变化大时比静态数组合适。

### 6.2.4 动态「左子/右兄」表示

这是本章的主实现：任意度树只需两个指针域。代价是不能 O(1) 取得「第 k 个孩子」。

```cpp file=code/ch06/general_tree/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::GeneralTree<char> tree;
    tree.create_root('A');
    auto* b = tree.insert_first(tree.root(), 'B');
    auto* c = tree.insert_next(b, 'C');
    tree.insert_next(c, 'D');
    tree.insert_first(b, 'E');
    tree.insert_next(tree.root()->child->child, 'F');

    std::cout << "先根: ";
    tree.preorder([](char value) { std::cout << value; });
    std::cout << "\n后根: ";
    tree.postorder([](char value) { std::cout << value; });
    std::cout << "\n层次: ";
    tree.breadth_first([](char value) { std::cout << value; });
    std::cout << '\n';

    dsa::DisjointSet sets(5);
    sets.unite(0, 1);
    sets.unite(1, 2);
    sets.unite(3, 4);
    std::cout << "0 与 2 同集合: " << (sets.same(0, 2) ? "是" : "否") << '\n';
    std::cout << "0 与 3 同集合: " << (sets.same(0, 3) ? "是" : "否") << '\n';
}
```

在仓库根目录运行：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch06/general_tree \
    code/ch06/general_tree/demo.cpp -o /tmp/tree-demo
/tmp/tree-demo
```

输出是：

```console
先根: ABEFCD
后根: EFBCDA
层次: ABCDEF
0 与 2 同集合: 是
0 与 3 同集合: 否
```

`insert_first(parent, value)` 把新结点插到孩子链的最前面；`insert_next(node, value)` 插到该结点的下一个兄弟。先插入 B，再 `insert_next(B, C)`、`insert_next(C, D)`，孩子从左到右就是 B、C、D。

`create_root` 清空旧树并新建根。`insert_first` 先让新结点的 `sibling` 指向原来的第一个孩子，再把它接到 `parent->child` 上——所以后插入的孩子会出现在兄弟链前端。`delete_subtree` 沿着父的孩子链或森林的根兄弟链找到指向它的指针，改写成下一个兄弟，再递归销毁。

孩子-兄弟树：

```cpp file=code/ch06/general_tree/modern.hpp#general-tree
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
```

### 6.2.5 父指针表示法和并查集

`find` 在返回根之前把沿途结点的父指针改成根，这就是路径压缩。`unite` 比较两棵树的秩，把较矮的挂到较高的下面；两棵一样高时，新根的秩加 1。

```cpp file=code/ch06/general_tree/modern.hpp#disjoint-set
class DisjointSet {
public:
    explicit DisjointSet(std::size_t count) : parent_(count), rank_(count, 0) {
        for (std::size_t index = 0; index < count; ++index) {
            parent_[index] = index;
        }
    }

    std::size_t find(std::size_t index) {
        if (index >= parent_.size()) {
            throw std::out_of_range("disjoint-set index");
        }
        if (parent_[index] != index) {
            parent_[index] = find(parent_[index]);
        }
        return parent_[index];
    }

    bool unite(std::size_t left, std::size_t right) {
        left = find(left);
        right = find(right);
        if (left == right) {
            return false;
        }
        if (rank_[left] < rank_[right]) {
            std::swap(left, right);
        }
        parent_[right] = left;
        if (rank_[left] == rank_[right]) {
            ++rank_[left];
        }
        return true;
    }

    [[nodiscard]] bool same(std::size_t left, std::size_t right) {
        return find(left) == find(right);
    }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> rank_;
};
```

## 6.3 树的顺序存储结构

当结点按某种周游次序排进数组时，还要额外记下「下一个兄弟在哪」或「有几个孩子」，才能还原树形。原书给了四种，思路没错，本章不另写一套未验证实现。

### 6.3.1 带右链的先根次序表示

按先根次序存放结点，每个结点另存「下一个兄弟」的下标。

### 6.3.2 带双标记的先根次序表示

两个布尔标记分别表示「有没有孩子」「有没有下一个兄弟」，比显式右链更省空间。

### 6.3.3 带度数的后根次序表示

按后根次序存放，每个结点记下度数。扫描时用栈按度数弹出孩子、合成子树。

### 6.3.4 带双标记的层次次序表示

按层存放，标记含义与 6.3.2 类似，只是次序改成层次周游。

## 6.4 K 叉树

每个结点至多 K 个孩子。满 K 叉树与完全 K 叉树的定义与二叉树平行：用编号 $Ki+1,\ldots,Ki+K$ 找孩子。K=2 就是二叉树。本章主实现仍是任意度的左子/右兄，不单独做一份 K 叉数组。
